#include "phoenix_mcp/server/task_store.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <random>
#include <utility>

namespace phoenix_mcp::server {

namespace {

using SteadyClock = std::chrono::steady_clock;

std::string format_iso8601(std::chrono::system_clock::time_point tp) {
  const std::time_t seconds = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &seconds);
#else
  gmtime_r(&seconds, &tm);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return std::string(buffer);
}

std::string now_iso8601() {
  return format_iso8601(std::chrono::system_clock::now());
}

}  // namespace

const char* to_string(TaskState state) {
  switch (state) {
    case TaskState::Working:
      return "working";
    case TaskState::InputRequired:
      return "input_required";
    case TaskState::Completed:
      return "completed";
    case TaskState::Failed:
      return "failed";
    case TaskState::Cancelled:
      return "cancelled";
  }
  return "working";
}

bool is_terminal(TaskState state) {
  return state == TaskState::Completed || state == TaskState::Failed ||
         state == TaskState::Cancelled;
}

TaskStore::TaskStore() : TaskStore(Options{}) {}

TaskStore::TaskStore(Options options) : options_(std::move(options)) {
  if (options_.max_tasks == 0) {
    options_.max_tasks = 1;
  }
  if (options_.max_ttl.count() <= 0) {
    options_.max_ttl = std::chrono::milliseconds(1);
  }
  if (options_.default_ttl.count() <= 0) {
    options_.default_ttl = std::chrono::milliseconds(1);
  }
  options_.default_ttl = std::min(options_.default_ttl, options_.max_ttl);
  if (options_.poll_interval.count() <= 0) {
    options_.poll_interval = std::chrono::milliseconds(1);
  }
}

std::optional<TaskSnapshot> TaskStore::create(
    std::optional<int64_t> requested_ttl_ms) {
  std::lock_guard lock(mutex_);
  const auto now = SteadyClock::now();
  purge_expired_locked(now);

  if (tasks_.size() >= options_.max_tasks && !evict_oldest_terminal_locked()) {
    return std::nullopt;
  }

  auto ttl = options_.default_ttl;
  if (requested_ttl_ms.has_value()) {
    const auto clamped =
        std::clamp<int64_t>(*requested_ttl_ms, 1, options_.max_ttl.count());
    ttl = std::chrono::milliseconds(clamped);
  }

  Entry entry;
  entry.snapshot.task_id = generate_task_id();
  entry.snapshot.state = TaskState::Working;
  entry.snapshot.created_at = now_iso8601();
  entry.snapshot.updated_at = entry.snapshot.created_at;
  entry.snapshot.ttl_ms = ttl.count();
  entry.snapshot.poll_interval_ms = options_.poll_interval.count();
  entry.expires_at = now + ttl;
  entry.sequence = next_sequence_++;

  const auto task_id = entry.snapshot.task_id;
  auto snapshot = entry.snapshot;
  tasks_.emplace(task_id, std::move(entry));
  return snapshot;
}

std::optional<TaskSnapshot> TaskStore::get(const std::string& task_id) {
  std::lock_guard lock(mutex_);
  const auto now = SteadyClock::now();
  purge_expired_locked(now);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return std::nullopt;
  }
  return it->second.snapshot;
}

std::vector<TaskSnapshot> TaskStore::list() {
  std::lock_guard lock(mutex_);
  purge_expired_locked(SteadyClock::now());

  std::vector<std::pair<uint64_t, TaskSnapshot>> by_sequence;
  by_sequence.reserve(tasks_.size());
  for (const auto& [task_id, entry] : tasks_) {
    by_sequence.emplace_back(entry.sequence, entry.snapshot);
  }
  std::sort(
      by_sequence.begin(), by_sequence.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

  std::vector<TaskSnapshot> ordered;
  ordered.reserve(by_sequence.size());
  for (auto& [sequence, snapshot] : by_sequence) {
    ordered.push_back(std::move(snapshot));
  }
  return ordered;
}

CancelStatus TaskStore::cancel(const std::string& task_id,
                               const std::string& reason, TaskSnapshot* out) {
  std::lock_guard lock(mutex_);
  purge_expired_locked(SteadyClock::now());

  const auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return CancelStatus::NotFound;
  }
  if (is_terminal(it->second.snapshot.state)) {
    if (out != nullptr) {
      *out = it->second.snapshot;
    }
    return CancelStatus::AlreadyTerminal;
  }

  it->second.cancellation.requestCancellation();
  it->second.snapshot.state = TaskState::Cancelled;
  it->second.snapshot.status_message = reason;
  it->second.snapshot.updated_at = now_iso8601();
  if (out != nullptr) {
    *out = it->second.snapshot;
  }
  cv_.notify_all();
  return CancelStatus::Cancelled;
}

void TaskStore::complete(const std::string& task_id, rfl::Generic result) {
  std::lock_guard lock(mutex_);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end() || is_terminal(it->second.snapshot.state)) {
    return;
  }
  it->second.snapshot.state = TaskState::Completed;
  it->second.snapshot.status_message.clear();
  it->second.snapshot.result = std::move(result);
  it->second.snapshot.updated_at = now_iso8601();
  cv_.notify_all();
}

void TaskStore::fail(const std::string& task_id, std::string status_message,
                     rfl::Generic result) {
  std::lock_guard lock(mutex_);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end() || is_terminal(it->second.snapshot.state)) {
    return;
  }
  it->second.snapshot.state = TaskState::Failed;
  it->second.snapshot.status_message = std::move(status_message);
  it->second.snapshot.result = std::move(result);
  it->second.snapshot.updated_at = now_iso8601();
  cv_.notify_all();
}

void TaskStore::fail_with_error(const std::string& task_id,
                                core::McpError error,
                                std::string status_message) {
  std::lock_guard lock(mutex_);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end() || is_terminal(it->second.snapshot.state)) {
    return;
  }
  it->second.snapshot.state = TaskState::Failed;
  it->second.snapshot.status_message = std::move(status_message);
  it->second.snapshot.error = std::move(error);
  it->second.snapshot.updated_at = now_iso8601();
  cv_.notify_all();
}

void TaskStore::mark_cancelled(const std::string& task_id, std::string reason) {
  std::lock_guard lock(mutex_);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end() || is_terminal(it->second.snapshot.state)) {
    return;
  }
  it->second.cancellation.requestCancellation();
  it->second.snapshot.state = TaskState::Cancelled;
  it->second.snapshot.status_message = std::move(reason);
  it->second.snapshot.updated_at = now_iso8601();
  cv_.notify_all();
}

std::optional<TaskSnapshot> TaskStore::wait_for_terminal(
    const std::string& task_id, std::chrono::milliseconds timeout) {
  std::unique_lock lock(mutex_);
  purge_expired_locked(SteadyClock::now());

  const auto deadline = SteadyClock::now() + timeout;
  cv_.wait_until(lock, deadline, [this, &task_id] {
    const auto it = tasks_.find(task_id);
    return it == tasks_.end() || is_terminal(it->second.snapshot.state);
  });

  const auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return std::nullopt;
  }
  return it->second.snapshot;
}

folly::CancellationToken TaskStore::token_for(
    const std::string& task_id) const {
  std::lock_guard lock(mutex_);
  const auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return folly::CancellationToken{};
  }
  return it->second.cancellation.getToken();
}

void TaskStore::cancel_all() {
  std::lock_guard lock(mutex_);
  const auto now = now_iso8601();
  for (auto& [task_id, entry] : tasks_) {
    if (is_terminal(entry.snapshot.state)) {
      continue;
    }
    entry.cancellation.requestCancellation();
    entry.snapshot.state = TaskState::Cancelled;
    entry.snapshot.status_message = "Session closed";
    entry.snapshot.updated_at = now;
  }
  cv_.notify_all();
}

size_t TaskStore::size() {
  std::lock_guard lock(mutex_);
  purge_expired_locked(SteadyClock::now());
  return tasks_.size();
}

void TaskStore::purge_expired_locked(SteadyClock::time_point now) {
  for (auto it = tasks_.begin(); it != tasks_.end();) {
    if (now >= it->second.expires_at) {
      it->second.cancellation.requestCancellation();
      it = tasks_.erase(it);
    } else {
      ++it;
    }
  }
}

bool TaskStore::evict_oldest_terminal_locked() {
  for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
    if (is_terminal(it->second.snapshot.state)) {
      tasks_.erase(it);
      return true;
    }
  }
  return false;
}

std::string TaskStore::generate_task_id() {
  static std::mutex rng_mutex;
  static std::random_device rng;

  std::array<uint8_t, 16> bytes{};
  {
    std::lock_guard lock(rng_mutex);
    for (auto& byte : bytes) {
      byte = static_cast<uint8_t>(rng());
    }
  }

  // RFC 4122 version 4 (random) UUID layout.
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

  char buffer[37];
  std::snprintf(buffer, sizeof(buffer),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                "%02x%02x%02x%02x%02x%02x",
                bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
                bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
                bytes[12], bytes[13], bytes[14], bytes[15]);
  return std::string(buffer);
}

}  // namespace phoenix_mcp::server
