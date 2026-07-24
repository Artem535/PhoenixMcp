#ifndef PHOENIX_MCP_SERVER_SESSION_MANAGER_H_
#define PHOENIX_MCP_SERVER_SESSION_MANAGER_H_

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <folly/coro/Task.h>

#include "phoenix_mcp/server/server_session.h"

namespace phoenix_mcp::server {

/// @brief Owns per-connection ServerSession instances.
///
/// Not wired into any transport yet: transports/McpRequestHandler still use a
/// single, process-lifetime ServerSession. SessionManager is the building
/// block for routing each connection to its own session, exercised directly
/// by tests until that wiring lands.
class SessionManager {
 public:
  /// Builds a fresh ServerSession for a newly accepted connection. Callers
  /// close over whatever ServerCapabilities/Implementation/ToolRegistry are
  /// shared across sessions (including any ServerConfig); SessionManager
  /// itself doesn't need to know how a session is constructed, only when to
  /// construct one.
  using SessionFactory = std::function<std::unique_ptr<ServerSession>()>;

  /// @param factory Builds one ServerSession per accepted connection.
  /// @param idle_timeout How long a session may go without activity before
  ///        expire_idle_sessions() considers it idle. 0 (default) disables
  ///        idle expiry.
  /// @param max_sessions Maximum sessions held at once. 0 (default) means
  ///        unlimited.
  explicit SessionManager(SessionFactory factory,
                          std::chrono::milliseconds idle_timeout =
                              std::chrono::milliseconds(0),
                          size_t max_sessions = 0);

  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  /// @brief Create and register a session for a new connection.
  /// @return Handle shared with the manager, or nullptr if `connection_id`
  ///         is already in use, `max_sessions` would be exceeded, or
  ///         shutdown_all() has already run.
  std::shared_ptr<ServerSession> create_session(
      const std::string& connection_id);

  /// @brief Look up an existing session by connection id.
  /// @return Handle shared with the manager, or nullptr if not found.
  std::shared_ptr<ServerSession> get_session(const std::string& connection_id);

  /// @brief Record activity on a session, resetting its idle timer.
  void touch_session(const std::string& connection_id);

  /// @brief Gracefully close and remove a session (e.g. on disconnect).
  void remove_session(const std::string& connection_id);

  /// @brief Gracefully close and remove any session idle for longer than
  ///        `idle_timeout`. No-op if `idle_timeout` is 0.
  void expire_idle_sessions();

  /// @brief Gracefully close every session and remove it from the manager.
  ///        After this returns, create_session() refuses new sessions.
  folly::coro::Task<void> shutdown_all();

  /// @brief Number of sessions currently held.
  size_t session_count() const;

 private:
  struct Entry {
    std::shared_ptr<ServerSession> session;
    std::chrono::steady_clock::time_point last_active;
  };

  SessionFactory factory_;
  std::chrono::milliseconds idle_timeout_;
  size_t max_sessions_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Entry> sessions_;
  bool stopped_ = false;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_SESSION_MANAGER_H_
