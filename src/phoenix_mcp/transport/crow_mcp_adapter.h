//
// Created by artem.d on 18.02.2026.
//

#pragma once

#include <cstdint>
#include <string>

#include "../constants/constants.hpp"
#include "../server/mcp_request_handler.h"

namespace pxm::server {

class CrowMcpAdapter {
public:
  struct Config {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    std::string endpoint = "/mcp";
    std::string health_endpoint = "/health";
  };

  explicit CrowMcpAdapter(McpRequestHandler& handler);
  CrowMcpAdapter(McpRequestHandler& handler, Config config);

  int run() const;

private:
  McpRequestHandler& handler_;
  Config config_;
};

} // namespace pxm::server
