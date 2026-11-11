#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <rfl/json.hpp>
#include <string>
#include <thread>
#include <utility>

#include "phoenix_mcp/server/server_session.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;
using namespace std::chrono_literals;

namespace {

constexpr auto kInit20251125 =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-11-25","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
constexpr auto kInit20250618 =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
constexpr auto kInitialized =
    R"({"jsonrpc":"2.0","method":"notifications/initialized"})";

std::unique_ptr<ServerSession> make_session(
    ServerConfig config = {},
    std::unique_ptr<phoenix_mcp::tool::ToolRegistry> registry = nullptr) {
  ServerCapabilities capabilities{.tools =
                                      ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  if (!registry) {
    registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  }
  return std::make_unique<ServerSession>(capabilities, info, "instruction",
                                         std::move(registry), config);
}

void initialize(ServerSession& session,
                const std::string& request = kInit20251125) {
  session.handle_input(request);
  session.handle_input(kInitialized);
}

std::string call_tool_with_task_json(int id, const std::string& name,
                                     const std::string& arguments_json,
                                     int ttl_ms = 60000) {
  return R"({"jsonrpc":"2.0","method":"tools/call","id":)" +
         std::to_string(id) + R"(,"params":{"name":")" + name +
         R"(","arguments":)" + arguments_json + R"(,"task":{"ttl":)" +
         std::to_string(ttl_ms) + R"(}}})";
}

std::string tasks_request_json(int id, const std::string& method,
                               const std::string& task_id) {
  return R"({"jsonrpc":"2.0","method":")" + method + R"(","id":)" +
         std::to_string(id) + R"(,"params":{"taskId":")" + task_id + R"("}})";
}

std::optional<rfl::Generic> response_result(
    const std::optional<rfl::Generic>& response) {
  if (!response.has_value()) {
    return std::nullopt;
  }
  const auto rpc = rfl::json::read<Response>(rfl::json::write(*response));
  if (!rpc.has_value()) {
    return std::nullopt;
  }
  return rpc->result;
}

std::optional<CreateTaskResult> created_task_from_response(
    const std::optional<rfl::Generic>& response) {
  const auto result = response_result(response);
  if (!result.has_value()) {
    return std::nullopt;
  }
  const auto created = rfl::from_generic<CreateTaskResult>(*result);
  if (!created.has_value()) {
    return std::nullopt;
  }
  return created.value();
}

std::optional<Task> task_from_response(
    const std::optional<rfl::Generic>& response) {
  const auto result = response_result(response);
  if (!result.has_value()) {
    return std::nullopt;
  }
  const auto task = rfl::from_generic<Task>(*result);
  if (!task.has_value()) {
    return std::nullopt;
  }
  return task.value();
}

struct EchoInput {
  std::string text;
};

std::unique_ptr<phoenix_mcp::tool::ToolRegistry> make_echo_registry() {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<EchoInput>(
      "echo", "Echo text", [](const EchoInput& input) {
        return CallToolResult{.content = {TextContent{.text = input.text}},
                              .is_error = false};
      });
  return registry;
}

}  // namespace

TEST(ServerSessionTasksTest, InitializeAdvertisesTasksFor20251125) {
  const auto session = make_session();
  const auto response = session->handle_input(kInit20251125);
  ASSERT_TRUE(response.has_value());
  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("\"tasks\":{"), std::string::npos);
  EXPECT_NE(json.find("\"requests\""), std::string::npos);
}

TEST(ServerSessionTasksTest, InitializeOmitsTasksFor20250618) {
  const auto session = make_session();
  const auto response = session->handle_input(kInit20250618);
  ASSERT_TRUE(response.has_value());
  const auto json = rfl::json::write(*response);
  EXPECT_EQ(json.find("\"tasks\":{"), std::string::npos);
}

TEST(ServerSessionTasksTest, InitializeOmitsTasksWhenDisabled) {
  ServerConfig config;
  config.enable_tasks = false;
  const auto session = make_session(config);
  const auto response = session->handle_input(kInit20251125);
  ASSERT_TRUE(response.has_value());
  const auto json = rfl::json::write(*response);
  EXPECT_EQ(json.find("\"tasks\":{"), std::string::npos);
}

namespace {

struct FailInput {
  int unused;
};

struct WaitInput {
  int poll_iterations;
};

}  // namespace

TEST(ServerSessionTasksTest, TaskAugmentedToolCallCompletes) {
  const auto session = make_session({}, make_echo_registry());
  initialize(*session);

  const auto created = created_task_from_response(session->handle_input(
      call_tool_with_task_json(10, "echo", R"({"text":"hello"})")));
  ASSERT_TRUE(created.has_value());
  const auto task_id = created->task.task_id.value();
  EXPECT_FALSE(task_id.empty());
  EXPECT_EQ(created->task.status, "working");

  std::optional<Task> task;
  for (int attempt = 0; attempt < 400; ++attempt) {
    task = task_from_response(
        session->handle_input(tasks_request_json(11, "tasks/get", task_id)));
    if (task.has_value() && task->status != "working") {
      break;
    }
    std::this_thread::sleep_for(2ms);
  }
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->status, "completed");

  const auto result = response_result(
      session->handle_input(tasks_request_json(12, "tasks/result", task_id)));
  ASSERT_TRUE(result.has_value());
  EXPECT_NE(rfl::json::write(*result).find("hello"), std::string::npos);
}

TEST(ServerSessionTasksTest, ToolErrorResultMarksTaskFailed) {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<FailInput>(
      "fail", "Always fails", [](const FailInput&) {
        return CallToolResult{.content = {TextContent{.text = "boom"}},
                              .is_error = true};
      });
  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  const auto created = created_task_from_response(session->handle_input(
      call_tool_with_task_json(20, "fail", R"({"unused":0})")));
  ASSERT_TRUE(created.has_value());
  const auto task_id = created->task.task_id.value();

  std::optional<Task> task;
  for (int attempt = 0; attempt < 400; ++attempt) {
    task = task_from_response(
        session->handle_input(tasks_request_json(21, "tasks/get", task_id)));
    if (task.has_value() && task->status != "working") {
      break;
    }
    std::this_thread::sleep_for(2ms);
  }
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->status, "failed");

  const auto result = response_result(
      session->handle_input(tasks_request_json(22, "tasks/result", task_id)));
  ASSERT_TRUE(result.has_value());
  const auto json = rfl::json::write(*result);
  EXPECT_NE(json.find("boom"), std::string::npos);
  EXPECT_NE(json.find("\"isError\":true"), std::string::npos);
}

TEST(ServerSessionTasksTest, TasksCancelStopsRunningTool) {
  auto started = std::make_shared<std::atomic<bool>>(false);
  auto saw_cancel = std::make_shared<std::atomic<bool>>(false);

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitInput>(
      "wait", "Wait until cancelled",
      [started, saw_cancel](const WaitInput& input,
                            const folly::CancellationToken& token)
          -> folly::coro::Task<CallToolResult> {
        started->store(true);
        for (int index = 0; index < input.poll_iterations; ++index) {
          if (token.isCancellationRequested()) {
            saw_cancel->store(true);
            co_return CallToolResult{
                .content = {TextContent{.text = "cancelled"}},
                .is_error = true};
          }
          std::this_thread::sleep_for(2ms);
        }
        co_return CallToolResult{.content = {TextContent{.text = "finished"}},
                                 .is_error = false};
      });
  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  const auto created = created_task_from_response(session->handle_input(
      call_tool_with_task_json(30, "wait", R"({"poll_iterations":500})")));
  ASSERT_TRUE(created.has_value());
  const auto task_id = created->task.task_id.value();

  for (int attempt = 0; attempt < 400 && !started->load(); ++attempt) {
    std::this_thread::sleep_for(2ms);
  }
  ASSERT_TRUE(started->load());

  const auto cancelled = task_from_response(
      session->handle_input(tasks_request_json(31, "tasks/cancel", task_id)));
  ASSERT_TRUE(cancelled.has_value());
  EXPECT_EQ(cancelled->status, "cancelled");

  for (int attempt = 0; attempt < 400 && !saw_cancel->load(); ++attempt) {
    std::this_thread::sleep_for(2ms);
  }
  EXPECT_TRUE(saw_cancel->load());
}

TEST(ServerSessionTasksTest, NotificationsCancelledCancelsTask) {
  auto started = std::make_shared<std::atomic<bool>>(false);
  auto saw_cancel = std::make_shared<std::atomic<bool>>(false);

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitInput>(
      "wait", "Wait until cancelled",
      [started, saw_cancel](const WaitInput& input,
                            const folly::CancellationToken& token)
          -> folly::coro::Task<CallToolResult> {
        started->store(true);
        for (int index = 0; index < input.poll_iterations; ++index) {
          if (token.isCancellationRequested()) {
            saw_cancel->store(true);
            co_return CallToolResult{
                .content = {TextContent{.text = "cancelled"}},
                .is_error = true};
          }
          std::this_thread::sleep_for(2ms);
        }
        co_return CallToolResult{.content = {TextContent{.text = "finished"}},
                                 .is_error = false};
      });
  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  const auto created = created_task_from_response(session->handle_input(
      call_tool_with_task_json(70, "wait", R"({"poll_iterations":500})")));
  ASSERT_TRUE(created.has_value());
  const auto task_id = created->task.task_id.value();

  for (int attempt = 0; attempt < 400 && !started->load(); ++attempt) {
    std::this_thread::sleep_for(2ms);
  }
  ASSERT_TRUE(started->load());

  session->handle_input(
      R"({"jsonrpc":"2.0","method":"notifications/cancelled",)"
      R"("params":{"requestId":70}})");

  for (int attempt = 0; attempt < 400 && !saw_cancel->load(); ++attempt) {
    std::this_thread::sleep_for(2ms);
  }
  EXPECT_TRUE(saw_cancel->load());

  const auto task = task_from_response(
      session->handle_input(tasks_request_json(71, "tasks/get", task_id)));
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->status, "cancelled");
}

TEST(ServerSessionTasksTest, TasksListReturnsCreatedTasks) {
  const auto session = make_session({}, make_echo_registry());
  initialize(*session);

  ASSERT_TRUE(
      created_task_from_response(session->handle_input(call_tool_with_task_json(
                                     40, "echo", R"({"text":"a"})")))
          .has_value());
  ASSERT_TRUE(
      created_task_from_response(session->handle_input(call_tool_with_task_json(
                                     41, "echo", R"({"text":"b"})")))
          .has_value());

  const auto result = response_result(session->handle_input(
      R"({"jsonrpc":"2.0","method":"tasks/list","id":42})"));
  ASSERT_TRUE(result.has_value());
  const auto list = rfl::from_generic<ListTasksResult>(*result);
  ASSERT_TRUE(list.has_value());
  EXPECT_GE(list->tasks.size(), 2u);
}

TEST(ServerSessionTasksTest, UnknownTaskReturnsError) {
  const auto session = make_session();
  initialize(*session);

  const auto response =
      session->handle_input(tasks_request_json(50, "tasks/get", "missing"));
  ASSERT_TRUE(response.has_value());
  const auto error = rfl::json::read<Error>(rfl::json::write(*response));
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->error.code, -32602);
}

TEST(ServerSessionTasksTest, TaskMetadataIgnoredWhenDisabled) {
  ServerConfig config;
  config.enable_tasks = false;
  const auto session = make_session(config, make_echo_registry());
  initialize(*session);

  const auto response = session->handle_input(
      call_tool_with_task_json(60, "echo", R"({"text":"hello"})"));
  ASSERT_TRUE(response.has_value());
  const auto json = rfl::json::write(*response);
  EXPECT_EQ(json.find("\"task\":{"), std::string::npos);
  EXPECT_NE(json.find("hello"), std::string::npos);
}
