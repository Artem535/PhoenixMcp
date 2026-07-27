#include "drogon_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "drogon_cancellation_state.h"

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define PXM_HAS_DROGON 1
#else
#define PXM_HAS_DROGON 0
#endif

namespace phoenix_mcp::server {

#if PXM_HAS_DROGON
namespace {

// Derives a stable per-TCP-connection key so repeated requests on the same
// (keep-alive) connection route to the same ServerSession. There is no MCP
// session-ID concept yet (that's Streamable HTTP, a later task).
//
// This deliberately does NOT use trantor::TcpConnection::setContext/
// getContext: that slot is a single shared `shared_ptr<void>` per
// connection, and Drogon itself (or other middleware) may already be using
// it for its own bookkeeping — reinterpreting whatever is stored there as a
// std::string via getContext<std::string>() reads garbage and corrupts
// memory (verified: this crashed with std::bad_alloc when tried). The peer
// address (ip:port) is a genuine, already-exposed-for-this-purpose identity
// for the connection, so it's used instead.
std::string connection_id_for(const drogon::HttpRequestPtr& req) {
  auto conn = req->getConnectionPtr().lock();
  if (!conn) {
    // No connection object available (e.g. some test doubles) — fall back
    // to a fresh id per request rather than crashing; this only means such
    // callers won't get session reuse across requests.
    static std::atomic<uint64_t> next_fallback_id{0};
    return "drogon-fallback-" + std::to_string(next_fallback_id.fetch_add(1));
  }
  return "drogon-" + conn->peerAddr().toIpPort();
}

}  // namespace
#endif

DrogonTransport::DrogonTransport() : DrogonTransport(Config{}) {}

DrogonTransport::DrogonTransport(Config cfg,
                                 std::shared_ptr<runtime::Runtime> runtime)
    : cfg_(std::move(cfg)),
      runtime_(std::move(runtime)),
      cancellation_state_(
          std::make_shared<drogon_internal::DrogonCancellationState>()) {}

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
      [on_message = std::move(on_message), runtime = runtime_,
       cancellation_state = cancellation_state_](
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
        request.connection_id = connection_id_for(req);
        const auto [cancel_token, first_request_on_connection] =
            cancellation_state->register_connection(request.connection_id);
        request.cancel_token = cancel_token;

        if (auto connection = req->getConnectionPtr().lock();
            first_request_on_connection && connection) {
          const auto previous_close_callback = connection->getCloseCallback();
          const auto connection_id = request.connection_id;
          connection->setCloseCallback(
              [cancellation_state, connection_id, previous_close_callback](
                  const trantor::TcpConnectionPtr& closed_connection) {
                cancellation_state->close(connection_id);
                if (previous_close_callback) {
                  previous_close_callback(closed_connection);
                }
              });
        }

        std::move(on_message(std::move(request)))
            .scheduleOn(runtime->cpu_executor())
            .start([callback = std::move(callback)](
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
