#include "phoenix_mcp/protocol/dialect.h"

#include <gtest/gtest.h>

#include <rfl/json.hpp>

using namespace phoenix_mcp::protocol;
using namespace phoenix_mcp::msg::types;
using namespace phoenix_mcp::core;

class Dialect2025_06_18Test : public ::testing::Test {
 protected:
  std::unique_ptr<ProtocolDialect> dialect_ = make_dialect_2025_06_18();
};

TEST_F(Dialect2025_06_18Test, Version) {
  EXPECT_EQ(dialect_->version(), "2025-06-18");
}

TEST_F(Dialect2025_06_18Test, CapabilityRules) {
  auto rules = dialect_->capability_rules();
  EXPECT_TRUE(rules.supports_tools);
  EXPECT_FALSE(rules.supports_resources);
  EXPECT_FALSE(rules.supports_prompts);
  EXPECT_FALSE(rules.supports_logging);
}

TEST_F(Dialect2025_06_18Test, DecodeInitializeRequest) {
  JsonRpcMessage msg =
      Request{.jsonrpc = "2.0",
              .method = "initialize",
              .id = 1,
              .params = rfl::Generic{rfl::Generic::Object{}}};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(std::holds_alternative<McpRequest>(result.value()));
  auto& req = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<InitializeCall>(req));
}

TEST_F(Dialect2025_06_18Test, DecodePingRequest) {
  JsonRpcMessage msg =
      Request{.jsonrpc = "2.0", .method = "ping", .id = 1};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& req = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<PingCall>(req));
}

TEST_F(Dialect2025_06_18Test, DecodeListToolsRequest) {
  JsonRpcMessage msg =
      Request{.jsonrpc = "2.0", .method = "tools/list", .id = 1};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& req = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<ListToolsCall>(req));
}

TEST_F(Dialect2025_06_18Test, DecodeCallToolRequest) {
  rfl::Generic::Object args;
  args["x"] = rfl::Generic{5};
  CallToolParams call_params{.name = "test_tool",
                             .arguments = args};
  JsonRpcMessage msg =
      Request{.jsonrpc = "2.0",
              .method = "tools/call",
              .id = 1,
              .params = rfl::to_generic(call_params)};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& req = std::get<McpRequest>(result.value());
  EXPECT_TRUE(std::holds_alternative<CallToolCall>(req));
}

TEST_F(Dialect2025_06_18Test, DecodeInitializedNotification) {
  JsonRpcMessage msg = Notification{
      .jsonrpc = "2.0", .method = "notifications/initialized"};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& notif = std::get<McpNotification>(result.value());
  EXPECT_TRUE(
      std::holds_alternative<InitializeNotificationCall>(notif));
}

TEST_F(Dialect2025_06_18Test, DecodeCancelNotification) {
  JsonRpcMessage msg = Notification{
      .jsonrpc = "2.0", .method = "notifications/cancelled"};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& notif = std::get<McpNotification>(result.value());
  EXPECT_TRUE(
      std::holds_alternative<CancelNotificationCall>(notif));
}

TEST_F(Dialect2025_06_18Test, DecodeToolListChangedNotification) {
  JsonRpcMessage msg = Notification{
      .jsonrpc = "2.0",
      .method = "notification/tools/listChanged"};
  auto result = dialect_->decode(msg);
  ASSERT_TRUE(result.hasValue());
  auto& notif = std::get<McpNotification>(result.value());
  EXPECT_TRUE(std::holds_alternative<
              ToolListChangedNotificationCall>(notif));
}

TEST_F(Dialect2025_06_18Test, DecodeUnknownMethod) {
  JsonRpcMessage msg =
      Request{.jsonrpc = "2.0", .method = "unknown", .id = 1};
  auto result = dialect_->decode(msg);
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(), ErrorCode::MethodNotFound);
}

TEST_F(Dialect2025_06_18Test, DecodeResponseFails) {
  JsonRpcMessage msg =
      Response{.jsonrpc = "2.0",
               .result = rfl::Generic{},
               .id = 1};
  auto result = dialect_->decode(msg);
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(), ErrorCode::InvalidRequest);
}

TEST_F(Dialect2025_06_18Test, EncodeResult) {
  McpRequest req = InitializeCall{
      InitializeRequest{Request{.jsonrpc = "2.0",
                                .method = "initialize",
                                .id = 1}}};
  rfl::Generic result_data{std::string("ok")};
  auto encoded = dialect_->encode_result(req, result_data);
  ASSERT_TRUE(encoded.hasValue());
  EXPECT_EQ(encoded.value().jsonrpc, "2.0");
}

TEST_F(Dialect2025_06_18Test, EncodeError) {
  McpError err(ErrorCode::MethodNotFound, "no such method");
  auto encoded = dialect_->encode_error(1, err);
  ASSERT_TRUE(encoded.hasValue());
  EXPECT_EQ(encoded.value().error.code, -32601);
  EXPECT_EQ(encoded.value().error.message, "no such method");
}

TEST_F(Dialect2025_06_18Test, MakeDialect2025_06_18) {
  auto d = make_dialect("2025-06-18");
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->version(), "2025-06-18");
}

TEST_F(Dialect2025_06_18Test, MakeDialectUnknownReturnsNull) {
  auto d = make_dialect("9999-99-99");
  EXPECT_EQ(d, nullptr);
}