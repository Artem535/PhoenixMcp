#include "phoenix_mcp/server/session_manager.h"

#include <utility>
#include <vector>

namespace phoenix_mcp::server {

SessionManager::SessionManager(SessionFactory factory,
                               std::chrono::milliseconds idle_timeout,
                               size_t max_sessions)
    : factory_(std::move(factory)),
      idle_timeout_(idle_timeout),
      max_sessions_(max_sessions) {}

std::shared_ptr<ServerSession> SessionManager::create_session(
    const std::string& connection_id) {
  // Built before locking so constructing a session (and its ToolRegistry)
  // never serializes every other connection's manager operations behind it;
  // the (rare) cost is a throwaway session if the id/limit/stopped checks
  // below reject it.
  auto session = std::shared_ptr<ServerSession>(factory_());

  std::lock_guard lock(mutex_);
  if (stopped_) {
    return nullptr;
  }
  if (max_sessions_ != 0 && sessions_.size() >= max_sessions_) {
    return nullptr;
  }
  auto [it, inserted] = sessions_.try_emplace(
      connection_id, Entry{session, std::chrono::steady_clock::now()});
  if (!inserted) {
    return nullptr;  // connection_id already has a session.
  }
  return session;
}

std::shared_ptr<ServerSession> SessionManager::get_session(
    const std::string& connection_id) {
  std::lock_guard lock(mutex_);
  const auto it = sessions_.find(connection_id);
  return it != sessions_.end() ? it->second.session : nullptr;
}

void SessionManager::touch_session(const std::string& connection_id) {
  std::lock_guard lock(mutex_);
  if (const auto it = sessions_.find(connection_id); it != sessions_.end()) {
    it->second.last_active = std::chrono::steady_clock::now();
  }
}

void SessionManager::remove_session(const std::string& connection_id) {
  std::shared_ptr<ServerSession> session;
  {
    std::lock_guard lock(mutex_);
    if (const auto it = sessions_.find(connection_id); it != sessions_.end()) {
      session = std::move(it->second.session);
      sessions_.erase(it);
    }
  }
  // close() can block for up to settle_timeout draining in-flight requests;
  // do it after releasing mutex_ so other connections aren't held up by it.
  // The shared_ptr keeps the session alive even if another thread is still
  // holding a handle obtained from an earlier get_session()/create_session().
  if (session) {
    session->close();
  }
}

void SessionManager::expire_idle_sessions() {
  if (idle_timeout_.count() == 0) {
    return;
  }

  std::vector<std::shared_ptr<ServerSession>> expired;
  {
    std::lock_guard lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (now - it->second.last_active > idle_timeout_) {
        expired.push_back(std::move(it->second.session));
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (auto& session : expired) {
    session->close();
  }
}

folly::coro::Task<void> SessionManager::shutdown_all() {
  std::vector<std::shared_ptr<ServerSession>> to_close;
  {
    std::lock_guard lock(mutex_);
    // Set under the same lock as the snapshot below so a create_session()
    // racing this call either inserts before this point (and gets closed
    // here) or sees stopped_ and refuses to insert at all — no session can
    // slip past both.
    stopped_ = true;
    for (auto& [id, entry] : sessions_) {
      to_close.push_back(std::move(entry.session));
    }
    sessions_.clear();
  }
  // Closed sequentially, each bounded by its own settle_timeout. This is a
  // coroutine for API composability (callers can co_await it alongside other
  // async shutdown steps), not because it currently runs concurrently —
  // making these closes genuinely parallel is follow-up work for whenever
  // this is wired into a transport with real concurrent sessions to shut
  // down.
  for (auto& session : to_close) {
    session->close();
  }
  co_return;
}

size_t SessionManager::session_count() const {
  std::lock_guard lock(mutex_);
  return sessions_.size();
}

}  // namespace phoenix_mcp::server
