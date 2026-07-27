#ifndef PHOENIX_MCP_TRANSPORT_STREAMABLE_HTTP_SESSION_STORE_H_
#define PHOENIX_MCP_TRANSPORT_STREAMABLE_HTTP_SESSION_STORE_H_

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace phoenix_mcp::server::streamable_http_internal {

enum class ReplayStatus { kOk, kResyncRequired };

struct StoredEvent {
  std::string id;
  std::string payload;
};

struct ReplayResult {
  ReplayStatus status = ReplayStatus::kResyncRequired;
  std::vector<StoredEvent> events;
};

class StreamableHttpSessionStore {
 public:
  using EventWriter = std::function<bool(const StoredEvent&)>;

  std::string create_session();
  bool contains_session(const std::string& session_id) const;
  void remove_session(const std::string& session_id);
  std::optional<std::string> create_stream(const std::string& session_id);
  std::optional<std::string> open_stream(const std::string& session_id,
                                         EventWriter writer);
  std::optional<StoredEvent> append(const std::string& session_id,
                                    const std::string& stream_id,
                                    std::string payload);
  ReplayResult replay(const std::string& session_id,
                      const std::string& event_id) const;
  std::optional<ReplayResult> resume_stream(const std::string& session_id,
                                            const std::string& event_id,
                                            EventWriter writer);
  bool publish(const std::string& session_id, std::string payload);

 private:
  struct Stream {
    size_t next_sequence = 1;
    size_t bytes = 0;
    std::deque<StoredEvent> events;
    EventWriter writer;
  };
  struct Session {
    size_t next_stream = 1;
    std::unordered_map<std::string, Stream> streams;
  };

  static constexpr size_t kMaxEvents = 1024;
  static constexpr size_t kMaxBytes = 4 * 1024 * 1024;
  static std::string make_id();
  static StoredEvent append_locked(Stream& stream, std::string payload);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, Session> sessions_;
};

}  // namespace phoenix_mcp::server::streamable_http_internal

#endif  // PHOENIX_MCP_TRANSPORT_STREAMABLE_HTTP_SESSION_STORE_H_
