//
// Created by artem.d on 18.02.2026.
//

#include "crow_mcp_adapter.h"

#include <utility>

#include <spdlog/spdlog.h>

#if __has_include(<crow.h>)
#include <crow.h>
#define PXM_HAS_CROW 1
#else
#define PXM_HAS_CROW 0
#endif

namespace pxm::server {

CrowMcpAdapter::CrowMcpAdapter(McpRequestHandler& handler)
    : CrowMcpAdapter(handler, Config{}) {
}

CrowMcpAdapter::CrowMcpAdapter(McpRequestHandler& handler, Config config)
    : handler_(handler), config_(std::move(config)) {
  if (config_.endpoint.empty() || config_.endpoint[0] != '/') {
    config_.endpoint.insert(config_.endpoint.begin(), '/');
  }

  if (config_.health_endpoint.empty() || config_.health_endpoint[0] != '/') {
    config_.health_endpoint.insert(config_.health_endpoint.begin(), '/');
  }
}

int CrowMcpAdapter::run() const {
#if PXM_HAS_CROW
  crow::SimpleApp app;

  app.route_dynamic(config_.health_endpoint)
      .methods(crow::HTTPMethod::GET)([] {
        return crow::response(200, "ok");
      });

  app.route_dynamic(config_.endpoint)
      .methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        if (req.body.empty()) {
          return crow::response(400, "Request body is empty");
        }

        const auto response = handler_.handle_json(req.body);
        if (!response.has_value()) {
          // MCP notifications don't require response payload.
          return crow::response(204);
        }

        crow::response resp{200, *response};
        resp.set_header("content-type", "application/json");
        return resp;
      });

  spdlog::info("CrowMcpAdapter| Listening on {}:{}{}",
               config_.bind_address, config_.port, config_.endpoint);

  app.concurrency(1)
      .bindaddr(config_.bind_address)
      .port(config_.port)
      .run();
  return constants::exit::Success;
#else
  spdlog::error(
      "CrowMcpAdapter| Crow headers were not found. Install Crow and add it "
      "to include paths.");
  return constants::exit::Error;
#endif
}

} // namespace pxm::server
