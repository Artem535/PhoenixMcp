#include "phoenix_mcp/server/mcp_request_handler.h"

#include <gtest/gtest.h>

#include <folly/coro/BlockingWait.h>

#include <memory>
#include <string>
#include <utility>

#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/server/server_message_sink.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

class RecordingMessageSink final : public ServerMessageSink {
 public:
  folly::coro::Task<bool> publish(std::string session_key,
                                  std::string json_rpc_message) override {
    session_key_ = std::move(session_key);
    json_rpc_message_ = std::move(json_rpc_message);
    co_return true;
  }

  std::string session_key_;
  std::string json_rpc_message_;
};

std::unique_ptr<McpRequestHandler> make_handler(
    std::shared_ptr<ServerMessageSink> message_sink = nullptr) {
  ServerCapabilities capabilities{
      .tools = ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  return std::make_unique<McpRequestHandler>(
      capabilities, info, "instruction",
      std::make_unique<phoenix_mcp::tool::ToolRegistry>(),
      std::move(message_sink));
}

constexpr auto kInitializeRequest =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";

ITransport::RequestEnvelope make_request(std::string body,
                                         std::string connection_id,
                                         ITransport::SessionLookupMode mode =
                                             ITransport::SessionLookupMode::ConnectionScoped) {
  ITransport::RequestEnvelope request;
  request.body = std::move(body);
  request.connection_id = std::move(connection_id);
  request.session_lookup_mode = mode;
  return request;
}

}  // namespace

TEST(McpRequestHandlerTest, DistinctConnectionIdsGetDistinctSessions) {
  auto handler = make_handler();

  // conn-a completes the initialize handshake.
  const auto init_response =
      handler->handle_json(make_request(kInitializeRequest, "conn-a"));
  ASSERT_TRUE(init_response.has_value());
  handler->handle_json(make_request(
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})",
      "conn-a"));

  // conn-b never sent initialize. If both connections shared one session,
  // this ping would succeed (served by conn-a's already-initialized
  // session); it must instead be rejected as a fresh, uninitialized
  // connection.
  const auto ping_response = handler->handle_json(
      make_request(R"({"jsonrpc":"2.0","method":"ping","id":2})", "conn-b"));
  ASSERT_TRUE(ping_response.has_value());
  EXPECT_NE(ping_response->body.find("Invalid request method"),
           std::string::npos)
      << ping_response->body;
}

// stdio never sets connection_id (it only ever serves one connection per
// process); this documents and locks in that every such request lands on the
// same single default session — SessionManager used as a single-entry
// manager, not bypassed — rather than each request silently starting a fresh
// session.
TEST(McpRequestHandlerTest, EmptyConnectionIdSharesOneSessionAcrossRequests) {
  auto handler = make_handler();

  const auto init_response =
      handler->handle_json(make_request(kInitializeRequest, ""));
  ASSERT_TRUE(init_response.has_value());
  handler->handle_json(make_request(
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})", ""));

  const auto ping_response = handler->handle_json(
      make_request(R"({"jsonrpc":"2.0","method":"ping","id":2})", ""));
  ASSERT_TRUE(ping_response.has_value());
  EXPECT_EQ(ping_response->body.find("Invalid request method"),
           std::string::npos)
      << ping_response->body;
}

TEST(McpRequestHandlerTest, HttpSessionBootstrapRetainsAcceptedInitialize) {
  auto handler = make_handler();

  const auto response = handler->handle_json(make_request(
      kInitializeRequest, "http-session-a",
      ITransport::SessionLookupMode::Bootstrap));

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->status_code, 0);

  const auto initialized = handler->handle_json(make_request(
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})",
      "http-session-a", ITransport::SessionLookupMode::ExistingOnly));
  EXPECT_FALSE(initialized.has_value());
}

TEST(McpRequestHandlerTest, HttpSessionRejectsMissingExistingSession) {
  auto handler = make_handler();

  const auto response = handler->handle_json(make_request(
      R"({"jsonrpc":"2.0","method":"ping","id":2})", "missing",
      ITransport::SessionLookupMode::ExistingOnly));

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->status_code, 404);
}

TEST(McpRequestHandlerTest, RemoveSessionMakesExistingOnlyRequestUnavailable) {
  auto handler = make_handler();
  ASSERT_TRUE(handler->handle_json(make_request(
      kInitializeRequest, "http-session-a",
      ITransport::SessionLookupMode::Bootstrap)));

  handler->remove_session("http-session-a");

  const auto response = handler->handle_json(make_request(
      R"({"jsonrpc":"2.0","method":"ping","id":2})", "http-session-a",
      ITransport::SessionLookupMode::ExistingOnly));
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->status_code, 404);
}

TEST(McpRequestHandlerTest, PublishToSessionUsesInjectedSink) {
  auto sink = std::make_shared<RecordingMessageSink>();
  auto handler = make_handler(sink);
  ASSERT_TRUE(handler->handle_json(make_request(
      kInitializeRequest, "http-session-a",
      ITransport::SessionLookupMode::Bootstrap)));

  EXPECT_TRUE(folly::coro::blockingWait(
      handler->publish_to_session("http-session-a", R"({"method":"ping"})")));
  EXPECT_EQ(sink->session_key_, "http-session-a");
  EXPECT_EQ(sink->json_rpc_message_, R"({"method":"ping"})");
}
