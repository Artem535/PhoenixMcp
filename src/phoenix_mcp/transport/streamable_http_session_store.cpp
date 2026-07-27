#include "streamable_http_session_store.h"

#include <folly/Random.h>

#include <array>
#include <string_view>

namespace phoenix_mcp::server::streamable_http_internal {
namespace {

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

}  // namespace

std::string StreamableHttpSessionStore::make_id() {
  std::string result;
  result.reserve(32);
  for (size_t index = 0; index < 32; ++index) {
    result.push_back(kAlphabet[folly::Random::secureRand32(kAlphabet.size())]);
  }
  return result;
}

std::string StreamableHttpSessionStore::create_session() {
  std::lock_guard lock(mutex_);
  std::string id;
  do {
    id = make_id();
  } while (sessions_.contains(id));
  sessions_.emplace(id, Session{});
  return id;
}

bool StreamableHttpSessionStore::contains_session(
    const std::string& session_id) const {
  std::lock_guard lock(mutex_);
  return sessions_.contains(session_id);
}

std::optional<std::string> StreamableHttpSessionStore::create_stream(
    const std::string& session_id) {
  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const std::string stream_id = "stream-" + std::to_string(session->second.next_stream++);
  session->second.streams.emplace(stream_id, Stream{});
  return stream_id;
}

std::optional<StoredEvent> StreamableHttpSessionStore::append(
    const std::string& session_id, const std::string& stream_id,
    std::string payload) {
  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const auto stream = session->second.streams.find(stream_id);
  if (stream == session->second.streams.end()) return std::nullopt;
  StoredEvent event{.id = stream_id + ":" + std::to_string(stream->second.next_sequence++),
                    .payload = std::move(payload)};
  stream->second.bytes += event.payload.size();
  stream->second.events.push_back(event);
  while (!stream->second.events.empty() &&
         (stream->second.events.size() > kMaxEvents ||
          stream->second.bytes > kMaxBytes)) {
    stream->second.bytes -= stream->second.events.front().payload.size();
    stream->second.events.pop_front();
  }
  return event;
}

ReplayResult StreamableHttpSessionStore::replay(const std::string& session_id,
                                                const std::string& event_id) const {
  std::lock_guard lock(mutex_);
  const auto separator = event_id.rfind(':');
  if (separator == std::string::npos) return {};
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return {};
  const auto stream = session->second.streams.find(event_id.substr(0, separator));
  if (stream == session->second.streams.end()) return {};
  ReplayResult result{.status = ReplayStatus::kOk};
  bool found = false;
  for (const auto& event : stream->second.events) {
    if (found) result.events.push_back(event);
    if (event.id == event_id) found = true;
  }
  if (!found) return {};
  return result;
}

}  // namespace phoenix_mcp::server::streamable_http_internal
