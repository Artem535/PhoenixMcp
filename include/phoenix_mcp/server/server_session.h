#ifndef PHOENIX_MCP_SERVER_SERVER_SESSION_H_
#define PHOENIX_MCP_SERVER_SERVER_SESSION_H_

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <folly/CancellationToken.h>
#include <folly/coro/Task.h>
#include <spdlog/spdlog.h>

#include "phoenix_mcp/core/error.h"
#include "phoenix_mcp/protocol/dialect.h"
#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/server/session_states.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"

namespace phoenix_mcp::server {

namespace msg_t = msg::types;
namespace protocol = phoenix_mcp::protocol;

/// @brief Configuration for ServerSession behavior
struct ServerConfig {
  /// Timeout for initialization handshake (default: 5s)
  std::chrono::milliseconds init_timeout{5000};
  /// Timeout for graceful shutdown settle period (default: 5s)
  std::chrono::milliseconds settle_timeout{5000};
};

/// @brief MCP server session with formal state machine lifecycle
///
/// Lifecycle: Uninitialized → Initializing → Operation → Stopping → Settled
/// Error path: Initializing → Failed → Stopping → Settled
///
/// One instance is meant to back a single connection; enforcing that
/// one-per-connection is `SessionManager`'s job. `SessionManager` doesn't
/// wire itself into any transport yet, so callers must not share one
/// ServerSession across connections themselves.
class ServerSession {
 public:
  explicit ServerSession(msg::types::ServerCapabilities server_capabilities,
                         msg::types::Implementation server_info,
                         std::string instruction,
                         std::unique_ptr<tool::ToolRegistry> tool_registry,
                         ServerConfig config = {});

  /// @brief Handle raw JSON input (request or notification)
  /// @param cancel_token Transport-level cancellation (e.g. client
  ///        disconnect); merged with any protocol-level cancellation
  ///        requested later via `notifications/cancelled`.
  /// @return Response JSON or nullopt for notifications
  std::optional<rfl::Generic> handle_input(
      const std::string& request, folly::CancellationToken cancel_token = {});
  folly::coro::Task<std::optional<rfl::Generic>> handle_input_async(
      std::string request, folly::CancellationToken cancel_token = {});

  /// @brief Handle structured request
  /// @return Response in rfl::Generic format
  rfl::Generic handle_request(const msg::types::Request& request,
                              folly::CancellationToken cancel_token = {});
  folly::coro::Task<rfl::Generic> handle_request_async(
      const msg::types::Request& request,
      folly::CancellationToken cancel_token = {});

  /// @brief Initiate graceful shutdown (idempotent)
  void close();

  /// @brief Check if session is in Operation state
  bool is_ready() const;

  /// @brief Get current state name (for diagnostics)
  std::string state_name() const;

  /// @brief Session configuration (read by FSM states for timeout values)
  const ServerConfig& config() const { return config_; }

 private:
  // ---- Configuration ----
  ServerConfig config_;

  // ---- Session data ----
  std::unique_ptr<tool::ToolRegistry> tool_registry_;
  msg::types::ServerCapabilities server_capabilities_;
  msg::types::Implementation server_info_;
  std::string instruction_;

  // ---- Protocol ----
  // The dialect used to decode/encode MCP messages. Bootstrapped to the
  // "2025-06-18" dialect at construction so the very first `initialize`
  // request (which is what negotiates the real dialect) can itself be
  // decoded; replaced with the negotiated dialect once `initialize`
  // succeeds. Never null.
  std::unique_ptr<protocol::ProtocolDialect> dialect_;

  // ---- State machine ----
  // Declared last: the FSM context is `*this`, and the initial state's
  // enter() may run during construction, so every other member must already
  // be fully constructed by the time `fsm_` is initialized.
  mutable std::mutex fsm_mutex_;
  SessionFSM::Instance fsm_;

  // Number of requests currently past the is_operation() admission check in
  // handle_request_async and not yet done executing. Incremented/decremented
  // by a PendingOperationGuard constructed *inside* the same fsm_mutex_
  // critical section as that check, so close()'s drain wait can never
  // observe "nothing pending" while a request that was already admitted
  // hasn't started running yet (that race existed when draining looked at
  // pending_cancellations_ alone, since that map is only populated later,
  // inside call_tool_async).
  std::atomic<int> pending_operation_count_{0};

  // ---- Cancellation ----
  // One CancellationSource per in-flight tools/call, keyed by request id, so
  // `notifications/cancelled` can cancel the specific request it names
  // without affecting any other request the session happens to be handling
  // concurrently. Other operation types (ping, tools/list) have no
  // per-request cancellation source — they're synchronous and never
  // suspend, so there's nothing useful to cancel — but they're still
  // counted in pending_operation_count_ above for drain purposes.
  std::mutex cancellations_mutex_;
  std::map<msg::types::RequestId, folly::CancellationSource>
      pending_cancellations_;

  void handle_cancel_notification(const msg::types::CancelNotification& notif);

  // ---- Internal handlers ----
  // Handles a decoded non-request envelope (a notification, per
  // JsonRpcCodec's JsonRpcMessage variant); decodes it through `dialect_`
  // and dispatches on the resulting McpNotification variant.
  void handle_notification(const protocol::JsonRpcMessage& envelope);

  folly::coro::Task<rfl::Generic> handle_operation_async(
      const msg::types::Request& request, folly::CancellationToken cancel_token);

  folly::coro::Task<rfl::Generic> call_tool_async(
      const msg::types::CallToolRequest& call_request,
      const msg::types::RequestId& id, folly::CancellationToken cancel_token);

  // ---- Helpers ----
  rfl::Generic make_initialize_response(const msg::types::RequestId& id) const;

  template <class T>
  rfl::Generic make_response(const T& result,
                             const msg::types::RequestId& id) const;

  // JSON-RPC 2.0 reserved error code for "Invalid params" (-32602). Kept as a
  // literal (rather than the private constants::msg_error enum) so this
  // public header stays self-contained for downstream consumers.
  static rfl::Generic create_error(const std::string& msg,
                                   const msg::types::RequestId& id,
                                   int code = -32602);

  // Encodes an McpError as a wire-format JSON-RPC error via `dialect_`, so
  // every error response — from a malformed envelope, a failed protocol
  // negotiation, or an unknown method — carries the exact category/code
  // JsonRpcCodec/ProtocolDialect themselves assign it, not a session-invented
  // one.
  rfl::Generic encode_error_response(const core::McpError& error,
                                     const msg::types::RequestId& id) const;

  // Decodes `initialize_request` through the bootstrap dialect to read the
  // client's requested protocolVersion, then resolves it via
  // `protocol::make_dialect`. Returns the negotiated dialect, or an McpError
  // (Protocol-category) if the request is malformed or the version is
  // unsupported. Does not mutate `dialect_` itself.
  folly::Expected<std::unique_ptr<protocol::ProtocolDialect>, core::McpError>
  negotiate_dialect(const msg::types::Request& initialize_request) const;

  // ---- State queries ----
  bool is_uninitialized() const;
  bool is_initializing() const;
  bool is_operation() const;
  bool is_failed() const;
  bool is_stopping() const;
  bool is_settled() const;

  // ---- FSM state-data access (keeps HFSM2's `access<TState>()` reach
  // confined to these two methods rather than spread through call sites) ----
  bool initialization_deadline_passed() const;
  void fail_initialization(const std::string& reason);
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_SERVER_SESSION_H_
