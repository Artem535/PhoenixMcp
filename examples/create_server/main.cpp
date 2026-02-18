//
// Created by artem.d on 12.11.2025.
//

#include <rfl/Generic.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/server/mcp_server.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.hpp"
#include "phoenix_mcp/transport/stdio_transport.h"

struct BasicToolInput {
  int a;
  int b;
};

struct BasicToolOutput {
  int mul_result;
};

auto sum_two_numbers(const BasicToolInput &input) {
  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  return pxm::utils::make_text_result(rfl::json::write(obj));
}

BasicToolOutput mul_two_numbers(const BasicToolInput &input) {
  return {.mul_result = input.a * input.b};
}

int main() {
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
    "./mcp_server.log", true);
  spdlog::set_default_logger(std::make_shared<spdlog::logger>(
    "stdio", spdlog::sinks_init_list{file_sink}));
  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);

  auto registry = std::make_unique<pxm::tool::ToolRegistry>();
  registry->register_tool<BasicToolInput>("sum_tool", "Sum two int numbers",
                                          sum_two_numbers);
  registry->register_tool<BasicToolInput, BasicToolOutput>(
    "mul_tool", "Mul two int numbers", mul_two_numbers);

  pxm::msg::types::ServerCapabilities caps{
    .tools = pxm::msg::types::ToolsCapabilities{.list_changed = false}
  };
  pxm::msg::types::Implementation info{
    .name = "My simple mcp server",
    .version = "1.0.0",
  };

  auto handler = std::make_unique<pxm::server::McpRequestHandler>(
    caps,
    info,
    "It is a simple mcp server for testing purposes",
    std::move(registry));
  auto transport = std::make_unique<pxm::server::StdioTransport>();

  pxm::server::McpServer server{std::move(transport), std::move(handler)};
  return server.run();
}
