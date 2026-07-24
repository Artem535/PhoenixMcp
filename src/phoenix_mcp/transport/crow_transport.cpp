#include "crow_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#if __has_include(<crow.h>)
#ifdef signal_add
#undef signal_add
#endif
#include <crow.h>
#define PXM_HAS_CROW 1
#else
#define PXM_HAS_CROW 0
#endif

namespace phoenix_mcp::server {

CrowTransport::CrowTransport() : CrowTransport(Config{}) {}

CrowTransport::CrowTransport(Config cfg,
                             std::shared_ptr<runtime::Runtime> runtime)
    : cfg_(std::move(cfg)), runtime_(std::move(runtime)) {}

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

  app.route_dynamic(health).methods(crow::HTTPMethod::GET)(
      [] { return crow::response(200, "ok"); });

  app.route_dynamic(endpoint).methods(crow::HTTPMethod::POST)(
      [&](const crow::request& req, crow::response& res) {
        if (req.body.empty()) {
          res.code = 400;
          res.write("Request body is empty");
          res.end();
          return;
        }

        ITransport::RequestEnvelope request;
        request.body = req.body;
        for (const auto& header : req.headers) {
          request.headers.emplace(header.first, header.second);
        }

        auto task = on_message(std::move(request));
        auto executor = runtime_->cpu_executor();
        executor->add([task = std::move(task), &res]() mutable {
          const auto response = folly::coro::blockingWait(std::move(task));
          if (!response.has_value()) {
            res.code = 204;
            res.end();
            return;
          }

          res.code = response->status_or(200);
          res.set_header("content-type", "application/json");
          res.write(response->body);
          res.end();
        });
      });

  spdlog::info("CrowTransport| Listening on {}:{}{}", cfg_.bind_address,
               cfg_.port, endpoint);

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

}  // namespace phoenix_mcp::server
