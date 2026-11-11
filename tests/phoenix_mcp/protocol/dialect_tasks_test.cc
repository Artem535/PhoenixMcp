#include <gtest/gtest.h>

#include <rfl/json.hpp>
#include <string>

#include "phoenix_mcp/protocol/dialect.h"

using namespace phoenix_mcp::protocol;
using namespace phoenix_mcp::msg::types;
using namespace phoenix_mcp::core;

namespace {

JsonRpcMessage task_id_request(const std::string& method,
                               const std::string& task_id) {
  return Request{.jsonrpc = "2.0",
                 .method = method,
                 .id = 1,
                 .params = rfl::to_generic(TaskIdParams{.task_id = task_id})};
}

}  // namespace

TEST(Dialect20251125TasksTest, CapabilityRulesAdvertiseTasks) {
  const auto dialect = make_dialect_2025_11_25();
  const auto rules = dialect->capability_rules();
  EXPECT_TRUE(rules.supports_tasks);
  EXPECT_TRUE(rules.supports_task_list);
  EXPECT_TRUE(rules.supports_task_cancel);
  EXPECT_TRUE(rules.supports_tools);
}

TEST(Dialect20251125TasksTest, DecodeTasksGet) {
  const auto dialect = make_dialect_2025_11_25();
  const auto result = dialect->decode(task_id_request("tasks/get", "abc"));
  ASSERT_TRUE(result.hasValue());
  const auto& message = std::get<McpRequest>(result.value());
  const auto* call = std::get_if<GetTaskCall>(&message);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->task_id, "abc");
}

TEST(Dialect20251125TasksTest, DecodeTasksResult) {
  const auto dialect = make_dialect_2025_11_25();
  const auto result = dialect->decode(task_id_request("tasks/result", "abc"));
  ASSERT_TRUE(result.hasValue());
  const auto& message = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<GetTaskResultCall>(message));
}

TEST(Dialect20251125TasksTest, DecodeTasksCancel) {
  const auto dialect = make_dialect_2025_11_25();
  const auto result = dialect->decode(task_id_request("tasks/cancel", "abc"));
  ASSERT_TRUE(result.hasValue());
  const auto& message = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<CancelTaskCall>(message));
}

TEST(Dialect20251125TasksTest, DecodeTasksList) {
  const auto dialect = make_dialect_2025_11_25();
  const Request request{
      .jsonrpc = "2.0",
      .method = "tasks/list",
      .id = 1,
      .params = rfl::to_generic(ListTasksParams{.cursor = "cursor-1"})};
  const auto result = dialect->decode(request);
  ASSERT_TRUE(result.hasValue());
  const auto& message = std::get<McpRequest>(result.value());
  const auto* call = std::get_if<ListTasksCall>(&message);
  ASSERT_NE(call, nullptr);
  ASSERT_TRUE(call->cursor.has_value());
  EXPECT_EQ(*call->cursor, "cursor-1");
}

TEST(Dialect20251125TasksTest, DecodeTaskStatusNotification) {
  const auto dialect = make_dialect_2025_11_25();
  const auto message = Notification{
      .jsonrpc = "2.0",
      .method = "notifications/tasks/status",
      .params = rfl::to_generic(Task{.task_id = "abc",
                                     .status = "completed",
                                     .created_at = "2025-11-25T10:00:00Z",
                                     .last_updated_at = "2025-11-25T10:01:00Z",
                                     .ttl = 1000,
                                     .poll_interval = 1000})};
  const auto result = dialect->decode(message);
  ASSERT_TRUE(result.hasValue());
  const auto& notification = std::get<McpNotification>(result.value());
  EXPECT_TRUE(std::holds_alternative<TaskStatusNotificationCall>(notification));
}

TEST(Dialect20251125TasksTest, DecodeCallToolWithTaskMetadata) {
  const auto dialect = make_dialect_2025_11_25();
  const CallToolParams params{.name = "echo",
                              .arguments = rfl::Generic::Object{},
                              .task = TaskMetadata{.ttl = 1000}};
  const auto message = Request{.jsonrpc = "2.0",
                               .method = "tools/call",
                               .id = 1,
                               .params = rfl::to_generic(params)};
  const auto result = dialect->decode(message);
  ASSERT_TRUE(result.hasValue());
  const auto& request = std::get<McpRequest>(result.value());
  const auto* call = std::get_if<CallToolCall>(&request);
  ASSERT_NE(call, nullptr);
  ASSERT_TRUE(call->request.params.has_value());
  const auto& call_params = call->request.params.value();
  ASSERT_TRUE(call_params.task.has_value());
  ASSERT_TRUE(call_params.task->ttl.has_value());
  EXPECT_EQ(*call_params.task->ttl, 1000);
}

TEST(Dialect20251125TasksTest, Dialect20250618RejectsTasksGet) {
  const auto dialect = make_dialect_2025_06_18();
  const auto result = dialect->decode(task_id_request("tasks/get", "abc"));
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(), ErrorCode::MethodNotFound);
}

TEST(Dialect20251125TasksTest, MakeDialectUnknownReturnsNull) {
  EXPECT_EQ(make_dialect("9999-99-99"), nullptr);
}
