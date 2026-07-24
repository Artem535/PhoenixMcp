#include "drogon_transport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

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
    return "drogon-fallback-" +
           std::to_string(next_fallback_id.fetch_add(1));
  }
  return "drogon-" + conn->peerAddr().toIpPort();
}

// Resolves (creating on first sight of `connection_id`) the
// CancellationSource for this connection, wiring it to fire when the
// connection actually closes, and returns its token.
//
// This chains onto the connection's existing close callback rather than
// overwriting it with conn->setCloseCallback(...) directly: that's a
// single shared callback slot, and Drogon (or other middleware) may
// already be using it for its own bookkeeping -- the same class of hazard
// as the setContext/getContext bug above, just for a different slot. The
// callback and the map it touches are only ever installed once per
// connection (on the first request), so a keep-alive connection handling
// many requests doesn't grow a callback chain per request; an uncancelled
// CancellationSource is safe to keep reusing across those requests.
folly::CancellationToken resolve_cancel_token(
    std::mutex& mutex,
    std::unordered_map<std::string, folly::CancellationSource>& sources,
    const std::string& connection_id, const drogon::HttpRequestPtr& req) {
  std::lock_guard lock(mutex);
  auto [it, inserted] = sources.try_emplace(connection_id);
  if (inserted) {
    if (auto conn = req->getConnectionPtr().lock()) {
      auto existing_close_cb = conn->getCloseCallback();
      conn->setCloseCallback(
          [&mutex, &sources, connection_id,
           existing_close_cb](const trantor::TcpConnectionPtr& c) {
            if (existing_close_cb) existing_close_cb(c);
            std::lock_guard inner_lock(mutex);
            if (auto found = sources.find(connection_id);
                found != sources.end()) {
              spdlog::info(
                  "DrogonTransport| connection '{}' closed, cancelling any "
                  "in-flight request on it",
                  connection_id);
              found->second.requestCancellation();
              sources.erase(found);
            }
          });
    }
  }
  return it->second.getToken();
}

}  // namespace
#endif

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
      [this, on_message = std::move(on_message), runtime = runtime_](
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
        request.cancel_token =
            resolve_cancel_token(cancel_sources_mutex_, cancel_sources_,
                                request.connection_id, req);

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
