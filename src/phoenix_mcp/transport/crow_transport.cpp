#include "crow_transport.h"

#include <spdlog/spdlog.h>

#if __has_include(<crow.h>)
#include <crow.h>
#define PXM_HAS_CROW 1
#else
#define PXM_HAS_CROW 0
#endif

namespace pxm::server {

CrowTransport::CrowTransport() : CrowTransport(Config{}) {
}

CrowTransport::CrowTransport(Config cfg) : cfg_(std::move(cfg)) {
}

int CrowTransport::run(Handler on_message) {
#if PXM_HAS_CROW
  crow::SimpleApp app;

  auto endpoint = cfg_.endpoint;
  if (endpoint.empty() || endpoint[0] != '/') {
    endpoint.insert(endpoint.begin(), '/');
  }

  auto health = cfg_.health_endpoint;
  if (health.empty() || health[0] != '/') {
    health.insert(health.begin(), '/');
  }

  app.route_dynamic(health)
      .methods(crow::HTTPMethod::GET)([] {
        return crow::response(200, "ok");
      });

  app.route_dynamic(endpoint)
      .methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
        if (req.body.empty()) {
          return crow::response(400, "Request body is empty");
        }

        const auto response = on_message(req.body);
        if (!response.has_value()) {
          return crow::response(204);
        }

        crow::response resp{200, *response};
        resp.set_header("content-type", "application/json");
        return resp;
      });

  spdlog::info("CrowTransport| Listening on {}:{}{}",
               cfg_.bind_address, cfg_.port, endpoint);

  app.concurrency(cfg_.concurrency)
      .bindaddr(cfg_.bind_address)
      .port(cfg_.port)
      .run();
  return 0;
#else
  spdlog::error("CrowTransport| Crow headers not found.");
  return 1;
#endif
}

} // namespace pxm::server
