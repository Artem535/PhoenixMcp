#include <folly/coro/BlockingWait.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "phoenix_mcp/constants/constants.hpp"
#include "phoenix_mcp/server/server_session.h"

namespace phoenix_mcp::server {

namespace cnt_error = constants::msg_error;

namespace {

msg::types::Task to_task_wire(const TaskSnapshot& snapshot) {
  const auto status_message =
      snapshot.status_message.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{snapshot.status_message};
  return msg::types::Task{
      .task_id = snapshot.task_id,
      .status = to_string(snapshot.state),
      .status_message = status_message,
      .created_at = snapshot.created_at,
      .last_updated_at = snapshot.updated_at,
      .ttl = snapshot.ttl_ms,
      .poll_interval = std::optional<int64_t>{snapshot.poll_interval_ms}};
}

rfl::Generic make_rpc_response(rfl::Generic result,
                               const msg::types::RequestId& id) {
  const msg::types::Response response{
      .jsonrpc = "2.0", .result = std::move(result), .id = id};
  return rfl::to_generic(response);
}

}  // namespace

std::string ServerSession::request_id_key(const msg::types::RequestId& id) {
  if (const auto* text = std::get_if<std::string>(&id)) {
    return "s:" + *text;
  }
  if (const auto* number = std::get_if<int>(&id)) {
    return "i:" + std::to_string(*number);
  }
  return {};
}

bool ServerSession::tasks_enabled() const {
  return config_.enable_tasks && task_store_ != nullptr &&
         dialect_->capability_rules().supports_tasks;
}

rfl::Generic ServerSession::start_task(
    const msg::types::CallToolRequest& call_request,
    const msg::types::RequestId& id) {
  const auto& params = call_request.params.value();
  const auto requested_ttl =
      params.task.has_value() ? params.task->ttl : std::optional<int64_t>{};

  auto created = task_store_->create(requested_ttl);
  if (!created.has_value()) {
    return create_error("Task capacity exceeded", id,
                        cnt_error::Code::Internal_error);
  }

  const auto task_id = created->task_id;
  {
    std::lock_guard lock(task_request_mutex_);
    request_to_task_[request_id_key(id)] = task_id;
  }

  auto store = task_store_;
  auto registry = tool_registry_;
  auto arguments = params.arguments.has_value()
                       ? params.arguments.value()
                       : rfl::Generic{rfl::Generic::Object{}};
  auto token = task_store_->token_for(task_id);
  auto name = params.name;

  std::thread([store = std::move(store), registry = std::move(registry),
               task_id, arguments = std::move(arguments),
               token = std::move(token), name = std::move(name)]() mutable {
    try {
      auto result = folly::coro::blockingWait(
          registry->call_tool_async(name, arguments, token));
      if (token.isCancellationRequested()) {
        store->mark_cancelled(task_id, "Request was cancelled");
      } else if (result.is_error.value().value_or(false)) {
        store->fail(task_id, "Tool returned an error", rfl::to_generic(result));
      } else {
        store->complete(task_id, rfl::to_generic(result));
      }
    } catch (const std::exception& error) {
      if (token.isCancellationRequested()) {
        store->mark_cancelled(task_id, "Request was cancelled");
      } else {
        store->fail_with_error(
            task_id,
            core::McpError(core::ErrorCode::ToolExecutionFailed, error.what()),
            error.what());
      }
    }
  }).detach();

  return make_task_created_response(*created, id);
}

rfl::Generic ServerSession::make_task_created_response(
    const TaskSnapshot& snapshot, const msg::types::RequestId& id) const {
  const msg::types::CreateTaskResult result{.task = to_task_wire(snapshot)};
  return make_rpc_response(rfl::to_generic(result), id);
}

rfl::Generic ServerSession::make_task_response(
    const TaskSnapshot& snapshot, const msg::types::RequestId& id) const {
  return make_rpc_response(rfl::to_generic(to_task_wire(snapshot)), id);
}

rfl::Generic ServerSession::handle_task_get(const std::string& task_id,
                                            const msg::types::RequestId& id) {
  const auto snapshot = task_store_->get(task_id);
  if (!snapshot.has_value()) {
    return create_error("Task not found", id, cnt_error::Code::Invalid_params);
  }
  return make_task_response(*snapshot, id);
}

rfl::Generic ServerSession::handle_task_result(
    const std::string& task_id, const msg::types::RequestId& id) {
  const auto snapshot =
      task_store_->wait_for_terminal(task_id, config_.task_result_timeout);
  if (!snapshot.has_value()) {
    return create_error("Task not found", id, cnt_error::Code::Invalid_params);
  }
  if (!is_terminal(snapshot->state)) {
    return create_error("Timed out waiting for task result", id,
                        cnt_error::Code::Internal_error);
  }
  if (snapshot->error.has_value()) {
    return encode_error_response(*snapshot->error, id);
  }
  if (snapshot->result.has_value()) {
    return make_rpc_response(*snapshot->result, id);
  }
  return encode_error_response(core::McpError(core::ErrorCode::Cancelled,
                                              snapshot->status_message.empty()
                                                  ? "Task was cancelled"
                                                  : snapshot->status_message),
                               id);
}

rfl::Generic ServerSession::handle_task_cancel(
    const std::string& task_id, const msg::types::RequestId& id) {
  TaskSnapshot snapshot;
  const auto outcome = task_store_->cancel(
      task_id, "The task was cancelled by request.", &snapshot);
  if (outcome == CancelStatus::NotFound) {
    return create_error("Task not found", id, cnt_error::Code::Invalid_params);
  }
  if (outcome == CancelStatus::AlreadyTerminal) {
    return create_error(
        std::string("Cannot cancel task: already in terminal status '") +
            to_string(snapshot.state) + "'",
        id, cnt_error::Code::Invalid_params);
  }
  return make_task_response(snapshot, id);
}

rfl::Generic ServerSession::handle_task_list(
    const msg::types::RequestId& id, const std::optional<std::string>& cursor) {
  constexpr size_t kPageSize = 50;

  size_t offset = 0;
  if (cursor.has_value()) {
    try {
      offset = static_cast<size_t>(std::stoull(*cursor));
    } catch (const std::exception&) {
      offset = 0;
    }
  }

  const auto all = task_store_->list();
  std::vector<msg::types::Task> tasks;
  for (size_t index = offset; index < all.size() && tasks.size() < kPageSize;
       ++index) {
    tasks.push_back(to_task_wire(all[index]));
  }

  std::optional<std::string> next_cursor;
  if (offset + tasks.size() < all.size()) {
    next_cursor = std::to_string(offset + tasks.size());
  }

  const msg::types::ListTasksResult result{.tasks = std::move(tasks),
                                           .next_cursor = next_cursor};
  return make_rpc_response(rfl::to_generic(result), id);
}

}  // namespace phoenix_mcp::server
