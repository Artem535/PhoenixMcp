//
// Created by artem.d on 18.02.2026.
//

#include <chrono>
#include <thread>

#include <folly/coro/Task.h>
#include <rfl/Generic.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/server/mcp_server.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.hpp"
#include "phoenix_mcp/transport/drogon_transport.h"

struct BasicToolInput {
  int a;
  int b;
};

struct DelayedToolInput {
  int a;
  int b;
  int delay_ms;
};

struct DelayedToolOutput {
  int sum;
  int delay_ms;
  bool async;
};

auto sum_two_numbers(const BasicToolInput &input) {
  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  return pxm::utils::make_text_result(rfl::json::write(obj));
}

folly::coro::Task<pxm::msg::types::CallToolResult> delayed_sum_tool(
    const DelayedToolInput& input) {
  std::this_thread::sleep_for(std::chrono::milliseconds(input.delay_ms));

  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  obj["delay_ms"] = input.delay_ms;
  co_return pxm::utils::make_text_result(rfl::json::write(obj));
}

folly::coro::Task<DelayedToolOutput> delayed_sum_struct_tool(
    const DelayedToolInput& input) {
  std::this_thread::sleep_for(std::chrono::milliseconds(input.delay_ms));
  co_return DelayedToolOutput{
      .sum = input.a + input.b,
      .delay_ms = input.delay_ms,
      .async = true,
  };
}

int main() {
  auto registry = std::make_unique<pxm::tool::ToolRegistry>();
  registry->register_tool<BasicToolInput>("sum_tool", "Sum two int numbers",
                                          sum_two_numbers);
  registry->register_async_tool<DelayedToolInput>(
      "delayed_sum_tool",
      "Sum two numbers after an artificial delay to demonstrate async HTTP handling",
      delayed_sum_tool,
      pxm::tool::ToolRegistry::ExecutionPolicy::IoBound);
  registry->register_async_tool<DelayedToolInput, DelayedToolOutput>(
      "delayed_sum_struct_tool",
      "Return a structured async result after an artificial delay",
      delayed_sum_struct_tool,
      pxm::tool::ToolRegistry::ExecutionPolicy::IoBound);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
    "./mcp_http_server.log", true);
  spdlog::set_default_logger(std::make_shared<spdlog::logger>(
    "http", spdlog::sinks_init_list{file_sink}));
  spdlog::set_level(spdlog::level::debug);

  pxm::msg::types::ServerCapabilities capabilities{
    .tools = pxm::msg::types::ToolsCapabilities{.list_changed = false}
  };
  pxm::msg::types::Implementation info{
    .name = "My async HTTP MCP server",
    .version = "1.0.0",
  };

  auto handler = std::make_unique<pxm::server::McpRequestHandler>(
    capabilities,
    info,
    "It is a simple HTTP MCP server for testing purposes",
    std::move(registry)
  );

  pxm::server::DrogonTransport::Config cfg{
    .bind_address = "0.0.0.0",
    .port = 8080,
    .endpoint = "/mcp",
    .health_endpoint = "/health",
    .concurrency = 4,
  };

  auto transport = std::make_unique<pxm::server::DrogonTransport>(cfg);
  pxm::server::McpServer server{std::move(transport), std::move(handler)};
  return server.run();
}
