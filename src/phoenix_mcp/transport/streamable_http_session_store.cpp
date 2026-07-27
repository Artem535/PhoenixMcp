#include "streamable_http_session_store.h"

#include <folly/Random.h>

#include <algorithm>
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

void StreamableHttpSessionStore::remove_session(const std::string& session_id) {
  std::lock_guard lock(mutex_);
  sessions_.erase(session_id);
}

std::optional<std::string> StreamableHttpSessionStore::create_stream(
    const std::string& session_id) {
  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const std::string stream_id =
      "stream-" + std::to_string(session->second.next_stream++);
  session->second.streams.emplace(stream_id, Stream{});
  return stream_id;
}

std::optional<std::string> StreamableHttpSessionStore::open_stream(
    const std::string& session_id, EventWriter writer) {
  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const std::string stream_id =
      "stream-" + std::to_string(session->second.next_stream++);
  auto [stream, inserted] =
      session->second.streams.emplace(stream_id, Stream{});
  stream->second.writer = std::move(writer);
  return stream_id;
}

StoredEvent StreamableHttpSessionStore::append_locked(Stream& stream,
                                                      std::string payload) {
  StoredEvent event{.id = "", .payload = std::move(payload)};
  stream.bytes += event.payload.size();
  stream.events.push_back(event);
  while (!stream.events.empty() &&
         (stream.events.size() > kMaxEvents || stream.bytes > kMaxBytes)) {
    stream.bytes -= stream.events.front().payload.size();
    stream.events.pop_front();
  }
  return event;
}

std::optional<StoredEvent> StreamableHttpSessionStore::append(
    const std::string& session_id, const std::string& stream_id,
    std::string payload) {
  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const auto stream = session->second.streams.find(stream_id);
  if (stream == session->second.streams.end()) return std::nullopt;
  auto event = append_locked(stream->second, std::move(payload));
  event.id = stream_id + ":" + std::to_string(stream->second.next_sequence++);
  stream->second.events.back().id = event.id;
  return event;
}

ReplayResult StreamableHttpSessionStore::replay(
    const std::string& session_id, const std::string& event_id) const {
  std::lock_guard lock(mutex_);
  const auto separator = event_id.rfind(':');
  if (separator == std::string::npos) return {};
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return {};
  const auto stream =
      session->second.streams.find(event_id.substr(0, separator));
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

std::optional<ReplayResult> StreamableHttpSessionStore::resume_stream(
    const std::string& session_id, const std::string& event_id,
    EventWriter writer) {
  std::lock_guard lock(mutex_);
  const auto separator = event_id.rfind(':');
  if (separator == std::string::npos) return std::nullopt;
  const auto session = sessions_.find(session_id);
  if (session == sessions_.end()) return std::nullopt;
  const auto stream =
      session->second.streams.find(event_id.substr(0, separator));
  if (stream == session->second.streams.end()) return std::nullopt;

  ReplayResult result{.status = ReplayStatus::kOk, .events = {}};
  bool found = false;
  for (const auto& event : stream->second.events) {
    if (found) result.events.push_back(event);
    if (event.id == event_id) found = true;
  }
  if (!found) return ReplayResult{};
  stream->second.writer = std::move(writer);
  return result;
}

bool StreamableHttpSessionStore::publish(const std::string& session_id,
                                         std::string payload) {
  EventWriter writer;
  std::string stream_id;
  StoredEvent event;
  {
    std::lock_guard lock(mutex_);
    const auto session = sessions_.find(session_id);
    if (session == sessions_.end()) return false;
    const auto stream =
        std::find_if(session->second.streams.begin(),
                     session->second.streams.end(), [](const auto& entry) {
                       return static_cast<bool>(entry.second.writer);
                     });
    if (stream == session->second.streams.end()) return false;
    stream_id = stream->first;
    event = append_locked(stream->second, std::move(payload));
    event.id = stream_id + ":" + std::to_string(stream->second.next_sequence++);
    stream->second.events.back().id = event.id;
    writer = stream->second.writer;
  }

  if (writer(event)) return true;

  std::lock_guard lock(mutex_);
  const auto session = sessions_.find(session_id);
  if (session != sessions_.end()) {
    const auto stream = session->second.streams.find(stream_id);
    if (stream != session->second.streams.end()) stream->second.writer = {};
  }
  return false;
}

}  // namespace phoenix_mcp::server::streamable_http_internal
