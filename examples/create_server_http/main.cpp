//
// Created by artem.d on 18.02.2026.
//

#include <rfl/Generic.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include "phoenix_mcp/constants/constants.hpp"
#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.hpp"
#include "phoenix_mcp/transport/crow_mcp_adapter.h"

struct BasicToolInput {
  int a;
  int b;
};

auto sum_two_numbers(const BasicToolInput& input) {
  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  return pxm::utils::make_text_result(rfl::json::write(obj));
}

int main() {
  auto registry = std::make_unique<pxm::tool::ToolRegistry>();
  registry->register_tool<BasicToolInput>("sum_tool", "Sum two int numbers",
                                          sum_two_numbers);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "./mcp_http_server.log", true);
  spdlog::set_default_logger(std::make_shared<spdlog::logger>(
      "http", spdlog::sinks_init_list{file_sink}));
  spdlog::set_level(spdlog::level::debug);

  pxm::msg::types::ServerCapabilities capabilities{
      .tools = pxm::msg::types::ToolsCapabilities{.list_changed = false}
  };
  pxm::msg::types::Implementation info{
      .name = "My HTTP MCP server",
      .version = "1.0.0",
  };

  pxm::server::McpRequestHandler handler{
      capabilities,
      info,
      "It is a simple HTTP MCP server for testing purposes",
      std::move(registry)
  };

  pxm::server::CrowMcpAdapter::Config cfg{
      .bind_address = "0.0.0.0",
      .port = 8080,
      .endpoint = "/mcp",
      .health_endpoint = "/health",
  };

  pxm::server::CrowMcpAdapter adapter{handler, cfg};
  return adapter.run();
}
