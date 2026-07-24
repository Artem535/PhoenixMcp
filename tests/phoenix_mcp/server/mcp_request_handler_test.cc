#include "phoenix_mcp/server/mcp_request_handler.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

std::unique_ptr<McpRequestHandler> make_handler() {
  ServerCapabilities capabilities{
      .tools = ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  return std::make_unique<McpRequestHandler>(
      capabilities, info, "instruction",
      std::make_unique<phoenix_mcp::tool::ToolRegistry>());
}

constexpr auto kInitializeRequest =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";

ITransport::RequestEnvelope make_request(std::string body,
                                         std::string connection_id) {
  ITransport::RequestEnvelope request;
  request.body = std::move(body);
  request.connection_id = std::move(connection_id);
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
