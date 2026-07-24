#include "drogon_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define PXM_HAS_DROGON 1
#else
#define PXM_HAS_DROGON 0
#endif

namespace phoenix_mcp::server {

DrogonTransport::DrogonTransport() : DrogonTransport(Config{}) {}

DrogonTransport::DrogonTransport(Config cfg,
                                 std::shared_ptr<runtime::Runtime> runtime)
    : cfg_(std::move(cfg)), runtime_(std::move(runtime)) {}

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
      [on_message = std::move(on_message), runtime = runtime_](
          const drogon::HttpRequestPtr& req,
          std::function<void(const drogon::HttpResponsePtr&)>&&
              callback) mutable {
        const auto body = req->body();
        if (body.empty()) {
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k400BadRequest);
          resp->setBody("Request body is empty");
          callback(resp);
          return;
        }

        ITransport::RequestEnvelope request;
        request.body = std::string(body);
        for (const auto& [header_name, header_value] : req->getHeaders()) {
          request.headers.emplace(header_name, header_value);
        }

        std::move(on_message(std::move(request)))
            .scheduleOn(runtime->cpu_executor())
            .start(
                [callback = std::move(callback)](
                    folly::Try<std::optional<ITransport::ResponseEnvelope>>&&
                        result) mutable {
                  auto resp = drogon::HttpResponse::newHttpResponse();
                  if (result.hasException()) {
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(result.exception().what().toStdString());
                    callback(resp);
                    return;
                  }

                  auto response = result.value();
                  if (!response.has_value()) {
                    resp->setStatusCode(drogon::k204NoContent);
                    callback(resp);
                    return;
                  }

                  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(
                      response->status_or(drogon::k200OK)));
                  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                  resp->setBody(response->body);
                  callback(resp);
                });
      },
      {drogon::Post});

  spdlog::info("DrogonTransport| Listening on {}:{}{}", cfg_.bind_address,
               cfg_.port, endpoint);

  app.run();
  return 0;
#else
  spdlog::error("DrogonTransport| Drogon headers not found.");
  return 1;
#endif
}

}  // namespace phoenix_mcp::server
