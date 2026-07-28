#include "drogon_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "drogon_cancellation_state.h"
#include "phoenix_mcp/server/server_message_sink.h"
#include "streamable_http_session_store.h"

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define PXM_HAS_DROGON 1
#else
#define PXM_HAS_DROGON 0
#endif

namespace phoenix_mcp::server {

#if PXM_HAS_DROGON
namespace {

using streamable_http_internal::StoredEvent;
using streamable_http_internal::StreamableHttpSessionStore;

std::string format_sse_event(const StoredEvent& event) {
  return "event: message\nid: " + event.id + "\ndata: " + event.payload +
         "\n\n";
}

bool accepts_sse(const drogon::HttpRequestPtr& request) {
  const auto accept = request->getHeader("accept");
  return accept.find("text/event-stream") != std::string::npos ||
         accept.find("*/*") != std::string::npos;
}

bool has_json_content_type(const drogon::HttpRequestPtr& request) {
  return request->getHeader("content-type").find("application/json") !=
         std::string::npos;
}

class DrogonServerMessageSink final : public ServerMessageSink {
 public:
  explicit DrogonServerMessageSink(
      std::shared_ptr<StreamableHttpSessionStore> session_store)
      : session_store_(std::move(session_store)) {}

  folly::coro::Task<bool> publish(std::string session_key,
                                  std::string json_rpc_message) override {
    co_return session_store_->publish(session_key, std::move(json_rpc_message));
  }

 private:
  std::shared_ptr<StreamableHttpSessionStore> session_store_;
};

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
          std::make_shared<drogon_internal::DrogonCancellationState>()),
      session_store_(std::make_shared<
                     streamable_http_internal::StreamableHttpSessionStore>()),
      message_sink_(std::make_shared<DrogonServerMessageSink>(session_store_)) {
}

std::shared_ptr<ServerMessageSink> DrogonTransport::message_sink() {
  return message_sink_;
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
      [on_message, runtime = runtime_, cancellation_state = cancellation_state_,
       session_store = session_store_](
          const drogon::HttpRequestPtr& req,
          std::function<void(const drogon::HttpResponsePtr&)>&&
              callback) mutable {
        if (!has_json_content_type(req)) {
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k415UnsupportedMediaType);
          callback(resp);
          return;
        }
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
        const auto tcp_connection_id = connection_id_for(req);
        const auto session_id = req->getHeader("mcp-session-id");
        if (!session_id.empty()) {
          if (!session_store->contains_session(session_id)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            callback(resp);
            return;
          }
          request.connection_id = session_id;
          request.session_lookup_mode =
              ITransport::SessionLookupMode::ExistingOnly;
        } else {
          request.connection_id = session_store->create_session();
          request.session_lookup_mode =
              ITransport::SessionLookupMode::Bootstrap;
        }
        const auto [cancel_token, first_request_on_connection] =
            cancellation_state->register_connection(tcp_connection_id);
        request.cancel_token = cancel_token;

        if (auto connection = req->getConnectionPtr().lock();
            first_request_on_connection && connection) {
          const auto previous_close_callback = connection->getCloseCallback();
          const auto connection_id = tcp_connection_id;
          connection->setCloseCallback(
              [cancellation_state, connection_id, previous_close_callback](
                  const trantor::TcpConnectionPtr& closed_connection) {
                cancellation_state->close(connection_id);
                if (previous_close_callback) {
                  previous_close_callback(closed_connection);
                }
              });
        }

        const auto logical_session_id = request.connection_id;
        const bool bootstrap = request.session_lookup_mode ==
                               ITransport::SessionLookupMode::Bootstrap;
        std::move(on_message(std::move(request)))
            .scheduleOn(runtime->cpu_executor())
            .start([callback = std::move(callback), session_store,
                    session_id = logical_session_id, bootstrap](
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
                if (bootstrap) {
                  session_store->remove_session(session_id);
                }
                resp->setStatusCode(drogon::k204NoContent);
                callback(resp);
                return;
              }

              resp->setStatusCode(static_cast<drogon::HttpStatusCode>(
                  response->status_or(drogon::k200OK)));
              if (bootstrap &&
                  response->status_or(drogon::k200OK) != drogon::k200OK) {
                session_store->remove_session(session_id);
              } else {
                resp->addHeader("Mcp-Session-Id", session_id);
              }
              resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
              resp->setBody(response->body);
              callback(resp);
            });
      },
      {drogon::Post});

  app.registerHandler(
      endpoint,
      [session_store = session_store_](
          const drogon::HttpRequestPtr& req,
          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        const auto session_id = req->getHeader("mcp-session-id");
        if (session_id.empty()) {
          auto response = drogon::HttpResponse::newHttpResponse();
          response->setStatusCode(drogon::k400BadRequest);
          callback(response);
          return;
        }
        if (!session_store->contains_session(session_id)) {
          auto response = drogon::HttpResponse::newHttpResponse();
          response->setStatusCode(drogon::k404NotFound);
          callback(response);
          return;
        }
        if (!accepts_sse(req)) {
          auto response = drogon::HttpResponse::newHttpResponse();
          response->setStatusCode(drogon::k406NotAcceptable);
          callback(response);
          return;
        }

        const auto last_event_id = req->getHeader("last-event-id");
        auto response = drogon::HttpResponse::newAsyncStreamResponse(
            [session_store, session_id,
             last_event_id](drogon::ResponseStreamPtr stream) {
              auto shared_stream =
                  std::shared_ptr<drogon::ResponseStream>(std::move(stream));
              const auto writer = [shared_stream](const StoredEvent& event) {
                return shared_stream->send(format_sse_event(event));
              };
              shared_stream->send("retry: 1000\n\n");

              if (last_event_id.empty()) {
                session_store->open_stream(session_id, writer);
                return;
              }

              const auto replay = session_store->resume_stream(
                  session_id, last_event_id, writer);
              if (!replay.has_value() ||
                  replay->status ==
                      streamable_http_internal::ReplayStatus::kResyncRequired) {
                shared_stream->send("event: resync-required\ndata: {}\n\n");
                shared_stream->close();
                return;
              }
              for (const auto& event : replay->events) {
                if (!shared_stream->send(format_sse_event(event))) return;
              }
            },
            true);
        response->setContentTypeString("text/event-stream");
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Mcp-Session-Id", session_id);
        callback(response);
      },
      {drogon::Get});

  app.registerHandler(
      endpoint,
      [on_message, runtime = runtime_, session_store = session_store_](
          const drogon::HttpRequestPtr& req,
          std::function<void(const drogon::HttpResponsePtr&)>&&
              callback) mutable {
        const auto session_id = req->getHeader("mcp-session-id");
        if (session_id.empty()) {
          auto response = drogon::HttpResponse::newHttpResponse();
          response->setStatusCode(drogon::k400BadRequest);
          callback(response);
          return;
        }
        if (!session_store->contains_session(session_id)) {
          auto response = drogon::HttpResponse::newHttpResponse();
          response->setStatusCode(drogon::k404NotFound);
          callback(response);
          return;
        }

        ITransport::RequestEnvelope request;
        request.connection_id = session_id;
        request.session_lookup_mode =
            ITransport::SessionLookupMode::ExistingOnly;
        request.operation = ITransport::RequestOperation::TerminateSession;
        std::move(on_message(std::move(request)))
            .scheduleOn(runtime->cpu_executor())
            .start([callback = std::move(callback), session_store, session_id](
                       folly::Try<std::optional<ITransport::ResponseEnvelope>>&&
                           result) mutable {
              auto response = drogon::HttpResponse::newHttpResponse();
              if (result.hasException()) {
                response->setStatusCode(drogon::k500InternalServerError);
              } else if (result.value().has_value()) {
                response->setStatusCode(static_cast<drogon::HttpStatusCode>(
                    result.value()->status_or(drogon::k204NoContent)));
              } else {
                response->setStatusCode(drogon::k500InternalServerError);
              }
              if (response->statusCode() == drogon::k204NoContent) {
                session_store->remove_session(session_id);
              }
              callback(response);
            });
      },
      {drogon::Delete});

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
