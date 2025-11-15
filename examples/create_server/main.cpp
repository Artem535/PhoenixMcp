//
// Created by artem.d on 12.11.2025.
//
#include <rfl/Generic.hpp>
#include "phoenix_mcp/server/server.h"
#include "phoenix_mcp/transport/stdio_transport.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

struct MyToolInput {
  int a;
  int b;
};

auto sum_two_numbers(const MyToolInput& input) {
  rfl::Generic::Object obj;
  obj["sum"] = input.a + input.b;
  return pxm::utils::make_text_result(rfl::json::write(obj));
}

int main() {
  auto registry = std::make_unique<pxm::tool::ToolRegistry>();
  registry->register_tool<MyToolInput>("sum_tool", "Sum two int numbers",
                                       sum_two_numbers);

  auto transport = std::make_unique<pxm::server::StdioTransport>();

  pxm::server::Server server{
      "My simple mcp server",
      "1.0.0",
      std::move(transport),
      std::move(registry),
      "It is a simple mcp server for testing purposes"
  };

  // Setup spdlog to log to a file
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
    "./mcp_server.log", true
  );

  spdlog::set_default_logger(std::make_shared<spdlog::logger>(
    "multi", spdlog::sinks_init_list{file_sink}
  ));

  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);

  spdlog::info("Test:{}", rfl::json::to_schema<MyToolInput>());
  return server.start_server();
}