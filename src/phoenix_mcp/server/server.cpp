//
// Created by artem.d on 09.11.2025.
//

#include "server.h"

#include <iostream>

namespace pxm::server {

Server::Server(std::string name, std::string desc,
               std::unique_ptr<AbstractTransport> transport,
               std::unique_ptr<tool::ToolRegistry>
               tool_registry) : name_(std::move(name)), desc_(std::move(desc)),
                                transport_(std::move(transport)),
                                tool_registry_(std::move(tool_registry)) {
  spdlog::info("Server::Server| Server created");
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
  tool_registry_ = std::move(tool_registry);
  // TODO: Add custom logic for make event to client
}


void Server::start_server_() {
  while (true) {
    std::string msg = transport_->read_msg();
    // Try convert string to init_req
    if (msg == "exit") {
      break;
    }
    transport_->write_msg(msg);
  }
}
}