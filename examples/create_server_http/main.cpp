//
// Created by artem.d on 18.02.2026.
//

#include <folly/coro/Task.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <chrono>
#include <rfl/Generic.hpp>
#include <thread>

#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/server/mcp_server.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.h"
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

auto sum_two_numbers(const BasicToolInput& input) {
  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  return phoenix_mcp::utils::make_text_result(rfl::json::write(obj));
}

folly::coro::Task<phoenix_mcp::msg::types::CallToolResult> delayed_sum_tool(
    const DelayedToolInput& input) {
  std::this_thread::sleep_for(std::chrono::milliseconds(input.delay_ms));

  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  obj["delay_ms"] = input.delay_ms;
  co_return phoenix_mcp::utils::make_text_result(rfl::json::write(obj));
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
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<BasicToolInput>("sum_tool", "Sum two int numbers",
                                          sum_two_numbers);
  registry->register_async_tool<DelayedToolInput>(
      "delayed_sum_tool",
      "Sum two numbers after an artificial delay to demonstrate async HTTP "
      "handling",
      delayed_sum_tool,
      phoenix_mcp::tool::ToolRegistry::ExecutionPolicy::IoBound);
  registry->register_async_tool<DelayedToolInput, DelayedToolOutput>(
      "delayed_sum_struct_tool",
      "Return a structured async result after an artificial delay",
      delayed_sum_struct_tool,
      phoenix_mcp::tool::ToolRegistry::ExecutionPolicy::IoBound);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "./mcp_http_server.log", true);
  spdlog::set_default_logger(std::make_shared<spdlog::logger>(
      "http", spdlog::sinks_init_list{file_sink}));
  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);

  phoenix_mcp::msg::types::ServerCapabilities capabilities{
      .tools =
          phoenix_mcp::msg::types::ToolsCapabilities{.list_changed = false}};
  phoenix_mcp::msg::types::Implementation info{
      .name = "My async HTTP MCP server",
      .version = "1.0.0",
  };

  auto handler = std::make_unique<phoenix_mcp::server::McpRequestHandler>(
      capabilities, info, "It is a simple HTTP MCP server for testing purposes",
      std::move(registry));

  phoenix_mcp::server::DrogonTransport::Config cfg{
      .bind_address = "0.0.0.0",
      .port = 8080,
      .endpoint = "/mcp",
      .health_endpoint = "/health",
      .concurrency = 4,
  };

  auto transport = std::make_unique<phoenix_mcp::server::DrogonTransport>(cfg);
  phoenix_mcp::server::McpServer server{std::move(transport),
                                        std::move(handler)};
  return server.run();
}
