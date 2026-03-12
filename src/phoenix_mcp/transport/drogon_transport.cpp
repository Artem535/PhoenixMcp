#include "drogon_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define PXM_HAS_DROGON 1
#else
#define PXM_HAS_DROGON 0
#endif

namespace pxm::server {

DrogonTransport::DrogonTransport() : DrogonTransport(Config{}) {
}

DrogonTransport::DrogonTransport(
    Config cfg, std::shared_ptr<runtime::Runtime> runtime)
    : cfg_(std::move(cfg)), runtime_(std::move(runtime)) {
}

int DrogonTransport::run(Handler on_message) {
#if PXM_HAS_DROGON
  auto endpoint = cfg_.endpoint;
  if (endpoint.empty() || endpoint[0] != '/') {
    endpoint.insert(endpoint.begin(), '/');
  }

  auto health = cfg_.health_endpoint;
  if (health.empty() || health[0] != '/') {
    health.insert(health.begin(), '/');
  }

  auto& app = drogon::app();
  app.addListener(cfg_.bind_address, cfg_.port);
  app.setThreadNum(cfg_.concurrency);

  app.registerHandler(
      health,
      [](const drogon::HttpRequestPtr&,
         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setBody("ok");
        callback(resp);
      },
      {drogon::Get});

  app.registerHandler(
      endpoint,
      [on_message = std::move(on_message), runtime = runtime_]
      (const drogon::HttpRequestPtr& req,
       std::function<void(const drogon::HttpResponsePtr&)>&& callback) mutable {
        const auto body = req->body();
        if (body.empty()) {
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k400BadRequest);
          resp->setBody("Request body is empty");
          callback(resp);
          return;
        }

        auto task = on_message(body);
        auto executor = runtime->cpu_executor();
        executor->add(
            [task = std::move(task), callback = std::move(callback)]() mutable {
              const auto response = folly::coro::blockingWait(std::move(task));
              auto resp = drogon::HttpResponse::newHttpResponse();
              if (!response.has_value()) {
                resp->setStatusCode(drogon::k204NoContent);
                callback(resp);
                return;
              }

              resp->setStatusCode(drogon::k200OK);
              resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
              resp->setBody(*response);
              callback(resp);
            });
      },
      {drogon::Post});

  spdlog::info("DrogonTransport| Listening on {}:{}{}",
               cfg_.bind_address, cfg_.port, endpoint);

  app.run();
  return 0;
#else
  spdlog::error("DrogonTransport| Drogon headers not found.");
  return 1;
#endif
}

} // namespace pxm::server
