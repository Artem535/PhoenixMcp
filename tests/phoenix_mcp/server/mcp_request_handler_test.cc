#include "phoenix_mcp/server/mcp_request_handler.h"

#include <gtest/gtest.h>

#include <memory>
#include <rfl/json.hpp>
#include <string>

namespace phoenix_mcp::server {
namespace {

std::unique_ptr<McpRequestHandler> make_handler() {
  msg::types::ServerCapabilities capabilities{
      .tools = msg::types::ToolsCapabilities{.list_changed = false}};
  msg::types::Implementation server_info{.name = "test-server",
                                         .version = "1.0.0"};

  return std::make_unique<McpRequestHandler>(
      capabilities, server_info, "test instructions",
      std::make_unique<tool::ToolRegistry>());
}

TEST(McpRequestHandlerTest, RequiresInitializedNotificationBeforeOperations) {
  auto handler = make_handler();

  const auto initialize_response = handler->handle_json(R"({
    "jsonrpc": "2.0",
    "method": "initialize",
    "id": 1
  })");
  ASSERT_TRUE(initialize_response.has_value());

  const auto initialize_result =
      rfl::json::read<msg::types::InitializeResultRPC>(*initialize_response);
  ASSERT_TRUE(initialize_result.has_value());
  EXPECT_EQ(initialize_result->result.server_info.get().name, "test-server");

  const auto before_initialized = handler->handle_json(R"({
    "jsonrpc": "2.0",
    "method": "ping",
    "id": 2
  })");
  ASSERT_TRUE(before_initialized.has_value());
  const auto before_initialized_error =
      rfl::json::read<msg::types::Error>(*before_initialized);
  ASSERT_TRUE(before_initialized_error.has_value());
  EXPECT_EQ(before_initialized_error->error.code, -32602);

  const auto initialized_response = handler->handle_json(R"({
    "jsonrpc": "2.0",
    "method": "notifications/initialized"
  })");
  EXPECT_FALSE(initialized_response.has_value());

  const auto ping_response = handler->handle_json(R"({
    "jsonrpc": "2.0",
    "method": "ping",
    "id": 3
  })");
  ASSERT_TRUE(ping_response.has_value());
  const auto ping_result =
      rfl::json::read<msg::types::Response>(*ping_response);
  ASSERT_TRUE(ping_result.has_value());
  EXPECT_EQ(std::get<int>(ping_result->id), 3);
}

TEST(McpRequestHandlerTest, ReturnsMethodNotFoundForUnknownMethod) {
  auto handler = make_handler();

  ASSERT_TRUE(
      handler->handle_json(R"({"jsonrpc":"2.0","method":"initialize","id":1})")
          .has_value());
  EXPECT_FALSE(
      handler
          ->handle_json(
              R"({"jsonrpc":"2.0","method":"notifications/initialized"})")
          .has_value());

  const auto response = handler->handle_json(
      R"({"jsonrpc":"2.0","method":"unknown/method","id":2})");
  ASSERT_TRUE(response.has_value());

  const auto error = rfl::json::read<msg::types::Error>(*response);
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->error.code, -32601);
  EXPECT_EQ(error->error.message, "Method not found: unknown/method");
}

}  // namespace
}  // namespace phoenix_mcp::server
