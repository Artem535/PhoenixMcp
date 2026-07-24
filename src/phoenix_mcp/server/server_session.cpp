#include "phoenix_mcp/server/server_session.h"

#include <chrono>
#include <thread>
#include <utility>

#include <folly/coro/BlockingWait.h>

#include "phoenix_mcp/constants/constants.hpp"

namespace phoenix_mcp::server {

namespace cnt_error = constants::msg_error;

namespace {

// RAII registration of a per-request CancellationSource: inserted into the
// session's pending-request map on construction, erased on destruction
// (including on exceptions), so a request's cancellation entry never
// outlives the request itself regardless of how it finishes.
class PendingRequestGuard {
 public:
  PendingRequestGuard(
      std::mutex& mutex,
      std::map<msg::types::RequestId, folly::CancellationSource>& pending,
      msg::types::RequestId id)
      : mutex_(mutex), pending_(pending), id_(std::move(id)) {
    std::lock_guard lock(mutex_);
    pending_[id_] = source_;
  }

  ~PendingRequestGuard() {
    std::lock_guard lock(mutex_);
    // Only erase our own entry: if a client reuses a request id while the
    // first request is still in flight, a second guard may have since
    // overwritten this one's map slot, and this destructor must not erase
    // that still-live entry. folly::CancellationSource has no operator==
    // definition in this folly version, so identity is checked via the
    // (defined) CancellationToken equality instead.
    if (const auto it = pending_.find(id_);
        it != pending_.end() && it->second.getToken() == token()) {
      pending_.erase(it);
    }
  }

  PendingRequestGuard(const PendingRequestGuard&) = delete;
  PendingRequestGuard& operator=(const PendingRequestGuard&) = delete;

  folly::CancellationToken token() const { return source_.getToken(); }

 private:
  std::mutex& mutex_;
  std::map<msg::types::RequestId, folly::CancellationSource>& pending_;
  msg::types::RequestId id_;
  folly::CancellationSource source_;
};

// RAII bump of the "operation in flight" counter close() drains against.
// Must be constructed inside the same fsm_mutex_ critical section as the
// is_operation() admission check (see handle_request_async) so there is no
// window where a request has been admitted but isn't yet accounted for.
class PendingOperationGuard {
 public:
  explicit PendingOperationGuard(std::atomic<int>& counter)
      : counter_(counter) {
    counter_.fetch_add(1, std::memory_order_relaxed);
  }

  ~PendingOperationGuard() { counter_.fetch_sub(1, std::memory_order_relaxed); }

  PendingOperationGuard(const PendingOperationGuard&) = delete;
  PendingOperationGuard& operator=(const PendingOperationGuard&) = delete;

 private:
  std::atomic<int>& counter_;
};

}  // namespace

// clang-format off
ServerSession::ServerSession(
    msg::types::ServerCapabilities server_capabilities,
    msg::types::Implementation server_info,
    std::string instruction,
    std::unique_ptr<tool::ToolRegistry> tool_registry,
    ServerConfig config)
    : config_(std::move(config)),
      tool_registry_(std::move(tool_registry)),
      server_capabilities_(std::move(server_capabilities)),
      server_info_(std::move(server_info)),
      instruction_(std::move(instruction)),
      dialect_(protocol::make_dialect_2025_06_18()),
      fsm_(*this) {}
// clang-format on

std::optional<rfl::Generic> ServerSession::handle_input(
    const std::string& request, folly::CancellationToken cancel_token) {
  return folly::coro::blockingWait(
      handle_input_async(request, std::move(cancel_token)));
}

folly::coro::Task<std::optional<rfl::Generic>> ServerSession::handle_input_async(
    std::string request, folly::CancellationToken cancel_token) {
  auto envelope = protocol::decode_message(request);
  if (!envelope) {
    co_return encode_error_response(envelope.error(), msg::types::RequestId{});
  }

  if (auto* req = std::get_if<msg::types::Request>(&envelope.value())) {
    co_return co_await handle_request_async(*req, std::move(cancel_token));
  }

  if (std::holds_alternative<msg::types::Notification>(envelope.value())) {
    handle_notification(envelope.value());
    co_return std::nullopt;
  }

  // decode_message can also yield Response/Error shapes (from the JSON-RPC
  // Response/Error grammar); a server never legitimately receives those from
  // a client, so treat it the same as an unrecognized envelope.
  co_return encode_error_response(
      core::McpError(core::ErrorCode::InvalidRequest,
                     "expected a JSON-RPC request or notification"),
      msg::types::RequestId{});
}

void ServerSession::handle_notification(
    const protocol::JsonRpcMessage& envelope) {
  auto decoded = dialect_->decode(envelope);
  if (!decoded) {
    spdlog::warn("ServerSession| Failed to decode notification: {}",
                 decoded.error().message());
    return;
  }

  auto* notification = std::get_if<protocol::McpNotification>(&decoded.value());
  if (!notification) return;

  if (auto* cancel =
          std::get_if<protocol::CancelNotificationCall>(notification)) {
    handle_cancel_notification(cancel->notification);
    return;
  }

  if (std::holds_alternative<protocol::InitializeNotificationCall>(
          *notification)) {
    std::lock_guard lock(fsm_mutex_);
    if (is_initializing()) {
      fsm_.react(InitializedNotification{});
    }
    return;
  }

  // ToolListChangedNotificationCall: server-originated in spirit; nothing for
  // the server to do if a client sends one back.
}

rfl::Generic ServerSession::handle_request(
    const msg::types::Request& request, folly::CancellationToken cancel_token) {
  return folly::coro::blockingWait(
      handle_request_async(request, std::move(cancel_token)));
}

folly::coro::Task<rfl::Generic> ServerSession::handle_request_async(
    const msg::types::Request& request, folly::CancellationToken cancel_token) {
  // The FSM checks/transitions below must stay under fsm_mutex_, but that
  // lock must not span the `co_await handle_operation_async(...)` at the
  // bottom: tool execution can hop onto a different executor thread
  // (ExecutionPolicy::CpuBound/IoBound), and unlocking a std::mutex from a
  // thread other than the one that locked it is undefined behavior. It would
  // also serialize every other request behind whichever tool call happens
  // to be in flight, defeating the point of per-request cancellation.
  std::optional<PendingOperationGuard> op_guard;
  {
    std::lock_guard lock(fsm_mutex_);

    // Check if we're in a state that accepts requests
    if (is_uninitialized()) {
      // Only accept initialize request
      if (request.method != msg_t::constants::initialize_request) {
        co_return create_error("Invalid request method", request.id);
      }

      // Transition into the handshake state first (mirrors the existing
      // "fail from Initializing" pattern used by the deadline check below);
      // negotiation failure below then fails out of Initializing, not
      // Uninitialized, since only Initializing reacts to TimeoutExpired.
      fsm_.react(InitializeRequest{});

      auto negotiated = negotiate_dialect(request);
      if (!negotiated) {
        fail_initialization(negotiated.error().message());
        co_return encode_error_response(negotiated.error(), request.id);
      }
      dialect_ = std::move(negotiated.value());

      co_return make_initialize_response(request.id);
    }

    if (is_initializing()) {
      if (initialization_deadline_passed()) {
        fail_initialization("Initialization handshake timed out");
        co_return create_error("Initialization handshake timed out",
                               request.id, cnt_error::Code::Invalid_request);
      }
      co_return create_error("Waiting for 'notifications/initialized'",
                             request.id);
    }

    if (is_failed()) {
      co_return create_error("Server is in failed state", request.id);
    }

    if (is_stopping() || is_settled()) {
      co_return create_error("Server is shutting down", request.id);
    }

    if (!is_operation()) {
      spdlog::error("ServerSession::handle_request| Invalid state");
      co_return create_error("Something went wrong", request.id);
    }

    // Counted from here (still holding fsm_mutex_) so close()'s drain wait
    // can never see zero pending operations while this request is about to
    // start running.
    op_guard.emplace(pending_operation_count_);
  }

  co_return co_await handle_operation_async(request, std::move(cancel_token));
}

void ServerSession::close() {
  std::chrono::steady_clock::time_point settle_deadline;
  {
    std::lock_guard lock(fsm_mutex_);
    if (!is_operation() && !is_failed()) {
      return;  // Idempotent: no-op if never started or already shutting down.
    }
    fsm_.react(ShutdownRequest{});
    settle_deadline = fsm_.access<states::Stopping>().settle_deadline;
  }

  // Signal every in-flight tools/call so cooperative tools can stop early,
  // then block (this call is synchronous, not a coroutine) until every
  // admitted operation finishes or the settle deadline passes — whichever
  // comes first. pending_operation_count_ (not pending_cancellations_,
  // which only covers tools/call) is the drain signal, since it's counted
  // from the same critical section as the admission check.
  {
    std::lock_guard lock(cancellations_mutex_);
    for (auto& [id, source] : pending_cancellations_) {
      source.requestCancellation();
    }
  }

  while (pending_operation_count_.load(std::memory_order_relaxed) > 0 &&
         std::chrono::steady_clock::now() < settle_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  std::lock_guard lock(fsm_mutex_);
  if (is_stopping()) {
    fsm_.react(SettleComplete{});
  }
}

bool ServerSession::is_ready() const {
  std::lock_guard lock(fsm_mutex_);
  return is_operation();
}

std::string ServerSession::state_name() const {
  std::lock_guard lock(fsm_mutex_);
  if (is_uninitialized()) return "Uninitialized";
  if (is_initializing()) return "Initializing";
  if (is_operation()) return "Operation";
  if (is_failed()) return "Failed";
  if (is_stopping()) return "Stopping";
  if (is_settled()) return "Settled";
  return "Unknown";
}

folly::coro::Task<rfl::Generic> ServerSession::handle_operation_async(
    const msg::types::Request& request, folly::CancellationToken cancel_token) {
  auto decoded = dialect_->decode(protocol::JsonRpcMessage{request});
  if (!decoded) {
    co_return encode_error_response(decoded.error(), request.id);
  }

  auto* mcp_request = std::get_if<protocol::McpRequest>(&decoded.value());
  if (!mcp_request) {
    co_return encode_error_response(
        core::McpError(core::ErrorCode::InvalidRequest, "expected a request"),
        request.id);
  }

  if (std::holds_alternative<protocol::InitializeCall>(*mcp_request)) {
    spdlog::info("ServerSession| Repeated initialize request received.");
    co_return make_initialize_response(request.id);
  }

  if (std::holds_alternative<protocol::PingCall>(*mcp_request)) {
    co_return make_response(msg_t::EmptyResult{}, request.id);
  }

  if (std::holds_alternative<protocol::ListToolsCall>(*mcp_request)) {
    const auto tool_list = tool_registry_->get_tool_list();
    const auto tool_list_res = msg_t::ListToolsResult{.tools = tool_list};
    co_return make_response(tool_list_res, request.id);
  }

  if (auto* call = std::get_if<protocol::CallToolCall>(mcp_request)) {
    co_return co_await call_tool_async(call->request, request.id,
                                       std::move(cancel_token));
  }

  spdlog::error("ServerSession| Method not found: {}", request.method);
  co_return encode_error_response(
      core::McpError(core::ErrorCode::MethodNotFound,
                     "Method not found: " + request.method),
      request.id);
}

folly::coro::Task<rfl::Generic> ServerSession::call_tool_async(
    const msg::types::CallToolRequest& call_request,
    const msg::types::RequestId& id, folly::CancellationToken cancel_token) {
  if (!call_request.params.has_value()) {
    co_return encode_error_response(
        core::McpError(core::ErrorCode::InvalidParams, "Invalid request"),
        id);
  }

  const auto& [name, arguments] = call_request.params.value();

  spdlog::debug("ServerSession::call_tool| Call tool, name: {}", name);
  spdlog::debug("ServerSession::call_tool| Call tool, args: {}",
                rfl::json::write(arguments));

  const PendingRequestGuard guard(cancellations_mutex_, pending_cancellations_,
                                  id);
  const auto merged_token =
      folly::cancellation_token_merge(cancel_token, guard.token());

  try {
    const auto result = co_await tool_registry_->call_tool_async(
        name, arguments.value(), merged_token);
    // A tool may return normally after observing cancellation (or ignore the
    // token entirely); surface cancellation uniformly here rather than
    // trusting each tool to encode it in its own result.
    if (merged_token.isCancellationRequested()) {
      co_return create_error("Request was cancelled", id,
                             cnt_error::Code::Request_cancelled);
    }
    co_return make_response(result, id);
  } catch (const std::exception& e) {
    spdlog::error("ServerSession::call_tool| Tool '{}' threw: {}", name,
                  e.what());
    co_return create_error(std::string("Tool execution failed: ") + e.what(),
                           id, cnt_error::Code::Internal_error);
  }
}

void ServerSession::handle_cancel_notification(
    const msg::types::CancelNotification& notif) {
  if (!notif.params.has_value()) {
    spdlog::warn("ServerSession| Malformed 'notifications/cancelled', ignoring");
    return;
  }

  const auto& params = notif.params.value();
  const msg_t::RequestId target_id = params.request_id.value();

  std::lock_guard lock(cancellations_mutex_);
  if (const auto it = pending_cancellations_.find(target_id);
      it != pending_cancellations_.end()) {
    it->second.requestCancellation();
    spdlog::info("ServerSession| Cancellation requested for in-flight request");
  }
}

folly::Expected<std::unique_ptr<protocol::ProtocolDialect>, core::McpError>
ServerSession::negotiate_dialect(
    const msg::types::Request& initialize_request) const {
  auto decoded =
      dialect_->decode(protocol::JsonRpcMessage{initialize_request});
  if (!decoded) {
    return folly::makeUnexpected(decoded.error());
  }

  auto* mcp_request = std::get_if<protocol::McpRequest>(&decoded.value());
  auto* init_call =
      mcp_request ? std::get_if<protocol::InitializeCall>(mcp_request)
                  : nullptr;
  if (!init_call) {
    return folly::makeUnexpected(core::McpError(
        core::ErrorCode::InvalidRequest, "expected an initialize request"));
  }

  const auto& raw_params = init_call->request.flatten.get().params;
  if (!raw_params.has_value()) {
    return folly::makeUnexpected(
        core::McpError(core::ErrorCode::InvalidParams,
                       "initialize request is missing params"));
  }

  const auto params =
      rfl::from_generic<msg_t::InitializeParams>(raw_params.value());
  if (!params) {
    return folly::makeUnexpected(
        core::McpError(core::ErrorCode::InvalidParams,
                       "initialize request has malformed params"));
  }

  const auto& requested_version = params.value().protocol_version.get();
  auto negotiated = protocol::make_dialect(requested_version);
  if (!negotiated) {
    return folly::makeUnexpected(core::McpError(
        core::ErrorCode::InvalidRequest,
        "unsupported protocol version: " + requested_version));
  }

  return negotiated;
}

rfl::Generic ServerSession::encode_error_response(
    const core::McpError& error, const msg::types::RequestId& id) const {
  auto encoded = dialect_->encode_error(id, error);
  if (!encoded) {
    return create_error(encoded.error().message(), id);
  }
  return rfl::to_generic(*encoded);
}

rfl::Generic ServerSession::make_initialize_response(
    const msg::types::RequestId& id) const {
  const msg::types::InitializeResult result{
      .protocol_version = constants::kMcpVersion,
      .capabilities = server_capabilities_,
      .server_info = server_info_,
      .instruction = instruction_};

  const msg::types::InitializeResultRPC resp{.id = id, .result = result};

  return rfl::to_generic(resp);
}

template <typename T>
rfl::Generic ServerSession::make_response(const T& result,
                                          const msg::types::RequestId& id) const {
  const msg::types::Response resp{
      .jsonrpc = "2.0", .result = rfl::to_generic(result), .id = id};

  return rfl::to_generic(resp);
}

rfl::Generic ServerSession::create_error(const std::string& msg,
                                         const msg::types::RequestId& id,
                                         const int code) {
  const msg::types::Error error{
      .id = id, .error = msg::types::ErrorData{.code = code, .message = msg}};

  return rfl::to_generic(error);
}

bool ServerSession::is_uninitialized() const {
  return fsm_.isActive<states::Uninitialized>();
}

bool ServerSession::is_initializing() const {
  return fsm_.isActive<states::Initializing>();
}

bool ServerSession::is_operation() const {
  return fsm_.isActive<states::Operation>();
}

bool ServerSession::is_failed() const {
  return fsm_.isActive<states::Failed>();
}

bool ServerSession::is_stopping() const {
  return fsm_.isActive<states::Stopping>();
}

bool ServerSession::is_settled() const {
  return fsm_.isActive<states::Settled>();
}

bool ServerSession::initialization_deadline_passed() const {
  return std::chrono::steady_clock::now() >
         fsm_.access<states::Initializing>().deadline;
}

void ServerSession::fail_initialization(const std::string& reason) {
  fsm_.react(TimeoutExpired{});
  fsm_.access<states::Failed>().reason = reason;
}

}  // namespace phoenix_mcp::server
