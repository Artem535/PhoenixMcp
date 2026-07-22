#include <gtest/gtest.h>

#include <rfl/json.hpp>

#include "phoenix_mcp/protocol/message_types.h"

using namespace phoenix_mcp::msg::types;

// ---------------------------------------------------------------------------
// Request parsing
// ---------------------------------------------------------------------------

TEST(JsonRpcMessageTest, ParseValidRequest) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "method": "tools/list",
    "id": 1
  })";

  const auto req = rfl::json::read<Request>(json);
  ASSERT_TRUE(req.has_value());
  EXPECT_EQ(req.value().method, "tools/list");
  EXPECT_TRUE(std::holds_alternative<int>(req.value().id));
  EXPECT_EQ(std::get<int>(req.value().id), 1);
}

TEST(JsonRpcMessageTest, ParseRequestWithParams) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "method": "tools/call",
    "id": 2,
    "params": {"name": "test_tool"}
  })";

  const auto req = rfl::json::read<Request>(json);
  ASSERT_TRUE(req.has_value());
  EXPECT_EQ(req.value().method, "tools/call");
  EXPECT_TRUE(req.value().params.has_value());
}

TEST(JsonRpcMessageTest, ParseRequestStringId) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "method": "ping",
    "id": "req-1"
  })";

  const auto req = rfl::json::read<Request>(json);
  ASSERT_TRUE(req.has_value());
  EXPECT_TRUE(std::holds_alternative<std::string>(req.value().id));
  EXPECT_EQ(std::get<std::string>(req.value().id), "req-1");
}

TEST(JsonRpcMessageTest, ParseRequestMissingMethod) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "id": 1
  })";

  const auto req = rfl::json::read<Request>(json);
  EXPECT_FALSE(req.has_value());
}

// ---------------------------------------------------------------------------
// Notification parsing
// ---------------------------------------------------------------------------

TEST(JsonRpcMessageTest, ParseValidNotification) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "method": "notifications/initialized"
  })";

  const auto notif = rfl::json::read<Notification>(json);
  ASSERT_TRUE(notif.has_value());
  EXPECT_EQ(notif.value().method, "notifications/initialized");
}

TEST(JsonRpcMessageTest, ParseNotificationWithoutMethod) {
  const auto json = R"({
    "jsonrpc": "2.0"
  })";

  const auto notif = rfl::json::read<Notification>(json);
  EXPECT_FALSE(notif.has_value());
}

// ---------------------------------------------------------------------------
// Response parsing
// ---------------------------------------------------------------------------

TEST(JsonRpcMessageTest, ParseValidResponse) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "result": {"tools": []},
    "id": 1
  })";

  const auto resp = rfl::json::read<Response>(json);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(std::get<int>(resp.value().id), 1);
}

TEST(JsonRpcMessageTest, ParseErrorResponse) {
  const auto json = R"({
    "jsonrpc": "2.0",
    "id": 1,
    "error": {"code": -32600, "message": "Invalid Request", "data": null}
  })";

  const auto err = rfl::json::read<Error>(json);
  ASSERT_TRUE(err.has_value());
  EXPECT_EQ(err.value().error.code, -32600);
  EXPECT_EQ(err.value().error.message, "Invalid Request");
}

// ---------------------------------------------------------------------------
// Invalid input
// ---------------------------------------------------------------------------

TEST(JsonRpcMessageTest, ParseMalformedJson) {
  const auto json = R"({invalid})";

  const auto req = rfl::json::read<Request>(json);
  EXPECT_FALSE(req.has_value());
}

TEST(JsonRpcMessageTest, ParseEmptyString) {
  const auto json = "";

  const auto req = rfl::json::read<Request>(json);
  EXPECT_FALSE(req.has_value());
}

TEST(JsonRpcMessageTest, ParseNonObjectJson) {
  const auto json = "42";

  const auto req = rfl::json::read<Request>(json);
  EXPECT_FALSE(req.has_value());
}