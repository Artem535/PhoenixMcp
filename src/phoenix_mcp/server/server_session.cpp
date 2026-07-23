#include "phoenix_mcp/server/server_session.h"

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
      fsm_(*this) {}
// clang-format on

std::optional<rfl::Generic> ServerSession::handle_input(
    const std::string& request, folly::CancellationToken cancel_token) {
  return folly::coro::blockingWait(
      handle_input_async(request, std::move(cancel_token)));
}

folly::coro::Task<std::optional<rfl::Generic>> ServerSession::handle_input_async(
    std::string request, folly::CancellationToken cancel_token) {
  if (const auto req = try_serialize_request(request); req.has_value())
    co_return co_await handle_request_async(*req, std::move(cancel_token));

  if (const auto notif = try_serialize_notification(request); notif.has_value()) {
    if (notif->method == msg_t::constants::cancel_notification) {
      handle_cancel_notification(*notif);
      co_return std::nullopt;
    }

    // Handle notifications based on current state
    std::lock_guard lock(fsm_mutex_);
    if (is_initializing() &&
        notif->method == msg_t::constants::initialize_notification) {
      fsm_.react(InitializedNotification{});
    }
    co_return std::nullopt;
  }

  spdlog::error("handle_input| Parsing Error");
  co_return std::nullopt;
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
  {
    std::lock_guard lock(fsm_mutex_);

    // Check if we're in a state that accepts requests
    if (is_uninitialized()) {
      // Only accept initialize request
      if (request.method != msg_t::constants::initialize_request) {
        co_return create_error("Invalid request method", request.id);
      }
      fsm_.react(InitializeRequest{});
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
  }

  co_return co_await handle_operation_async(request, std::move(cancel_token));
}

void ServerSession::close() {
  std::lock_guard lock(fsm_mutex_);
  if (is_operation() || is_failed()) {
    fsm_.react(ShutdownRequest{});
    // No in-flight-request tracking yet (that lands with the SessionManager
    // work in Phase 3), so there is nothing to drain — settle immediately.
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
  if (request.method == msg_t::constants::initialize_request) {
    spdlog::info("ServerSession| Repeated initialize request received.");
    co_return make_initialize_response(request.id);
  }

  if (request.method == msg_t::constants::ping_request) {
    co_return make_response(msg_t::EmptyResult{}, request.id);
  }

  if (request.method == msg_t::constants::list_tools_request) {
    const auto tool_list = tool_registry_->get_tool_list();
    const auto tool_list_res = msg_t::ListToolsResult{.tools = tool_list};
    co_return make_response(tool_list_res, request.id);
  }

  if (request.method == msg_t::constants::call_tool_request) {
    co_return co_await call_tool_async(request, std::move(cancel_token));
  }

  spdlog::error("ServerSession| Method not found: {}", request.method);
  co_return create_error("Method not found: " + request.method, request.id,
                         cnt_error::Code::Invalid_request);
}

folly::coro::Task<rfl::Generic> ServerSession::call_tool_async(
    const msg::types::Request& request, folly::CancellationToken cancel_token) {
  spdlog::debug("ServerSession::call_tool| Call tool {}",
                rfl::json::write(request));

  const auto generic = rfl::to_generic(request);
  const auto [flatten, opt_params] =
      rfl::from_generic<msg_t::CallToolRequest>(generic).value();

  if (!opt_params.has_value()) {
    co_return create_error("Invalid request", request.id);
  }

  const auto [name, arguments] = opt_params.value();

  spdlog::debug("ServerSession::call_tool| Call tool, name: {}", name);
  spdlog::debug("ServerSession::call_tool| Call tool, args: {}",
                rfl::json::write(arguments));

  const PendingRequestGuard guard(cancellations_mutex_, pending_cancellations_,
                                  request.id);
  const auto merged_token =
      folly::cancellation_token_merge(cancel_token, guard.token());

  try {
    const auto result = co_await tool_registry_->call_tool_async(
        name, arguments.value(), merged_token);
    // A tool may return normally after observing cancellation (or ignore the
    // token entirely); surface cancellation uniformly here rather than
    // trusting each tool to encode it in its own result.
    if (merged_token.isCancellationRequested()) {
      co_return create_error("Request was cancelled", request.id,
                             cnt_error::Code::Request_cancelled);
    }
    co_return make_response(result, request.id);
  } catch (const std::exception& e) {
    spdlog::error("ServerSession::call_tool| Tool '{}' threw: {}", name,
                  e.what());
    co_return create_error(std::string("Tool execution failed: ") + e.what(),
                           request.id, cnt_error::Code::Internal_error);
  }
}

void ServerSession::handle_cancel_notification(
    const msg::types::Notification& notif) {
  const auto generic = rfl::to_generic(notif);
  const auto parsed = rfl::from_generic<msg_t::CancelNotification>(generic);
  if (!parsed.has_value() || !parsed.value().params.has_value()) {
    spdlog::warn("ServerSession| Malformed 'notifications/cancelled', ignoring");
    return;
  }

  const auto& params = parsed.value().params.value();
  const msg_t::RequestId target_id = params.request_id.value();

  std::lock_guard lock(cancellations_mutex_);
  if (const auto it = pending_cancellations_.find(target_id);
      it != pending_cancellations_.end()) {
    it->second.requestCancellation();
    spdlog::info("ServerSession| Cancellation requested for in-flight request");
  }
}

std::optional<msg::types::Request> ServerSession::try_serialize_request(
    const std::string& request) {
  std::optional<msg::types::Request> request_ = std::nullopt;
  try {
    request_ = rfl::json::read<msg::types::Request>(request).value();
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize request: {}", e.what());
  }
  return request_;
}

std::optional<msg::types::Notification>
ServerSession::try_serialize_notification(const std::string& json) {
  try {
    return rfl::json::read<msg::types::Notification>(json).value();
  } catch (...) {
    return std::nullopt;
  }
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
