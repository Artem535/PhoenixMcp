#ifndef PHOENIX_MCP_SERVER_SERVER_SESSION_H_
#define PHOENIX_MCP_SERVER_SERVER_SESSION_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <folly/CancellationToken.h>
#include <folly/coro/Task.h>
#include <spdlog/spdlog.h>

#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/server/session_states.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"

namespace phoenix_mcp::server {

namespace msg_t = msg::types;

using OptionalRequest = std::optional<msg::types::Request>;
using OptionalNotification = std::optional<msg::types::Notification>;

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
/// one-per-connection is the SessionManager's job (Phase 3 — not yet wired
/// up), so callers must not share one ServerSession across connections yet.
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

  // ---- State machine ----
  // Declared last: the FSM context is `*this`, and the initial state's
  // enter() may run during construction, so every other member must already
  // be fully constructed by the time `fsm_` is initialized.
  mutable std::mutex fsm_mutex_;
  SessionFSM::Instance fsm_;

  // ---- Cancellation ----
  // One CancellationSource per in-flight request, keyed by request id, so
  // `notifications/cancelled` can cancel the specific request it names
  // without affecting any other request the session happens to be handling
  // concurrently (HTTP transports can dispatch overlapping requests into the
  // same session even before Phase 3's per-connection SessionManager lands).
  std::mutex cancellations_mutex_;
  std::map<msg::types::RequestId, folly::CancellationSource>
      pending_cancellations_;

  void handle_cancel_notification(const msg::types::Notification& notif);

  // ---- Internal handlers ----
  folly::coro::Task<rfl::Generic> handle_operation_async(
      const msg::types::Request& request, folly::CancellationToken cancel_token);

  folly::coro::Task<rfl::Generic> call_tool_async(
      const msg::types::Request& request, folly::CancellationToken cancel_token);

  // ---- Helpers ----
  static OptionalRequest try_serialize_request(const std::string& request);
  static OptionalNotification try_serialize_notification(
      const std::string& json);

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
