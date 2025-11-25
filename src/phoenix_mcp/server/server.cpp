//
// Created by artem.d on 09.11.2025.
//

#include "server.h"

#include <iostream>
#include <utility>

namespace pxm::server {

Server::Server(std::string name, std::string version,
               std::unique_ptr<AbstractTransport> transport,
               std::unique_ptr<tool::ToolRegistry> tool_registry,
               std::string instruction) : transport_(std::move(transport)) {
  spdlog::info("Server::Server| Server created");
  server_info_ = {
      .name = std::move(name),
      .version = std::move(version),
  };

  // Now we support only tools. Without change event system
  server_capabilities_ = {
      .tools = msg::types::ToolsCapabilities{.list_changed = false}
  };

  instruction_ = std::move(instruction);

  session_ = std::make_unique<McpSession>(server_capabilities_, server_info_,
                                          instruction_,
                                          std::move(tool_registry));

}

int Server::start_server() {
  try {
    spdlog::info("Server::start_server| Start server");
    start_server_();
  } catch (std::exception& e) {
    spdlog::error("Server::start_server| Error: {}", e.what());
    return cnt::exit::Error;
  }

  spdlog::info("Server::start_server| Server stopped");
  return cnt::exit::Success;
}

void Server::change_tool_registry(
    std::unique_ptr<tool::ToolRegistry> tool_registry) {
  spdlog::info("Server::change_tool_registry| Change tool registry");
  // TODO: Add custom logic for make event to client
}


void Server::start_server_() {
  while (true) {
    std::string json = transport_->read_msg();
    spdlog::debug("Server::start_server_| Read message: {}", json);

    if (json.empty()) {
      spdlog::info("Server| Empty message, shutdown server");
      break;
    }

    if (const auto result = session_->handle_input(json); result.has_value()) {
      const auto res_str = rfl::json::write(result.value());
      spdlog::debug("Server::start_server_| Write message: {}", res_str);
      transport_->write_msg(res_str);
    }
  }
}
}