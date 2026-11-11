#ifndef PHOENIX_MCP_SERVER_TASK_STORE_H_
#define PHOENIX_MCP_SERVER_TASK_STORE_H_

#include <folly/CancellationToken.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <rfl/Generic.hpp>
#include <string>
#include <vector>

#include "phoenix_mcp/core/error.h"

namespace phoenix_mcp::server {

/// @brief Lifecycle status of an MCP task (SEP-1686, protocol 2025-11-25).
enum class TaskState {
  Working,
  InputRequired,
  Completed,
  Failed,
  Cancelled,
};

/// @brief Wire-format status string for a task state.
const char* to_string(TaskState state);

/// @brief True for states a task can never leave.
bool is_terminal(TaskState state);

/// @brief Point-in-time view of a task, safe to copy out of the store.
struct TaskSnapshot {
  std::string task_id;
  TaskState state = TaskState::Working;
  std::string status_message;
  std::string created_at;
  std::string updated_at;
  std::optional<int64_t> ttl_ms;
  int64_t poll_interval_ms = 1000;
  std::optional<rfl::Generic> result;
  std::optional<core::McpError> error;
};

enum class CancelStatus {
  NotFound,
  Cancelled,
  AlreadyTerminal,
};

/// @brief Thread-safe, bounded store for in-flight and completed MCP tasks.
///
/// Tasks are scoped to one `ServerSession` (one connection) and are removed
/// once their TTL elapses, whether or not they reached a terminal status.
class TaskStore {
 public:
  struct Options {
    size_t max_tasks = 64;
    std::chrono::milliseconds default_ttl{600000};
    std::chrono::milliseconds max_ttl{3600000};
    std::chrono::milliseconds poll_interval{1000};
  };

  TaskStore();
  explicit TaskStore(Options options);

  TaskStore(const TaskStore&) = delete;
  TaskStore& operator=(const TaskStore&) = delete;

  /// @brief Create a task in `working` status.
  /// @return A snapshot of the new task, or nullopt when the store is at
  ///         capacity and no terminal task can be evicted.
  std::optional<TaskSnapshot> create(std::optional<int64_t> requested_ttl_ms);

  /// @brief Look up a live task. Expired tasks are purged first.
  std::optional<TaskSnapshot> get(const std::string& task_id);

  /// @brief List live tasks ordered by creation time.
  std::vector<TaskSnapshot> list();

  /// @brief Cancel a task.
  /// @param out Receives the resulting snapshot (terminal or cancelled).
  CancelStatus cancel(const std::string& task_id, const std::string& reason,
                      TaskSnapshot* out);

  /// @brief Mark a task completed with the underlying request result.
  void complete(const std::string& task_id, rfl::Generic result);

  /// @brief Mark a task failed while retaining the underlying result.
  ///
  /// Used for tool calls whose result carries `isError: true`: the task
  /// status is `failed`, but `tasks/result` still returns that result.
  void fail(const std::string& task_id, std::string status_message,
            rfl::Generic result);

  /// @brief Mark a task failed with a JSON-RPC error to replay on
  /// `tasks/result`.
  void fail_with_error(const std::string& task_id, core::McpError error,
                       std::string status_message);

  /// @brief Transition a task to `cancelled` unless it is already terminal.
  void mark_cancelled(const std::string& task_id, std::string reason);

  /// @brief Block until the task is terminal, the timeout elapses, or the
  ///        task disappears (expired). Returns the final snapshot.
  std::optional<TaskSnapshot> wait_for_terminal(
      const std::string& task_id, std::chrono::milliseconds timeout);

  /// @brief Cancellation token used to stop the task's underlying work.
  folly::CancellationToken token_for(const std::string& task_id) const;

  /// @brief Request cancellation of every live task (session shutdown).
  void cancel_all();

  size_t size();

 private:
  struct Entry {
    TaskSnapshot snapshot;
    std::chrono::steady_clock::time_point expires_at;
    folly::CancellationSource cancellation;
    uint64_t sequence = 0;
  };

  Options options_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::map<std::string, Entry> tasks_;
  uint64_t next_sequence_ = 0;

  void purge_expired_locked(std::chrono::steady_clock::time_point now);
  bool evict_oldest_terminal_locked();
  static std::string generate_task_id();
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_TASK_STORE_H_
