#include "phoenix_mcp/protocol/json_rpc_codec.h"

#include <gtest/gtest.h>

using namespace phoenix_mcp::protocol;
using namespace phoenix_mcp::msg::types;

TEST(JsonRpcCodecTest, DecodeValidRequest) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"tools/list","id":1})";
  auto result = decode_request(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value().method, "tools/list");
  EXPECT_EQ(std::get<int>(result.value().id), 1);
}

TEST(JsonRpcCodecTest, DecodeRequestWithParams) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"test"}})";
  auto result = decode_request(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value().method, "tools/call");
  EXPECT_TRUE(result.value().params.has_value());
}

TEST(JsonRpcCodecTest, DecodeRequestStringId) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"ping","id":"req-1"})";
  auto result = decode_request(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(std::get<std::string>(result.value().id), "req-1");
}

TEST(JsonRpcCodecTest, DecodeRequestMissingMethod) {
  const auto json = R"({"jsonrpc":"2.0","id":1})";
  auto result = decode_request(json);
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::DeserializationFailed);
}

TEST(JsonRpcCodecTest, DecodeValidNotification) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
  auto result = decode_notification(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value().method, "notifications/initialized");
}

TEST(JsonRpcCodecTest, DecodeValidResponse) {
  const auto json =
      R"({"jsonrpc":"2.0","result":{"tools":[]},"id":1})";
  auto result = decode_response(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(std::get<int>(result.value().id), 1);
}

TEST(JsonRpcCodecTest, DecodeErrorResponse) {
  const auto json =
      R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request","data":null}})";
  auto result = decode_error(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value().error.code, -32600);
  EXPECT_EQ(result.value().error.message, "Invalid Request");
}

TEST(JsonRpcCodecTest, DecodeMessageRequest) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"initialize","id":1})";
  auto result = decode_message(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(
      std::holds_alternative<Request>(result.value()));
}

TEST(JsonRpcCodecTest, DecodeMessageNotification) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
  auto result = decode_message(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(
      std::holds_alternative<Notification>(result.value()));
}

TEST(JsonRpcCodecTest, DecodeMessageResponse) {
  const auto json =
      R"({"jsonrpc":"2.0","result":{},"id":1})";
  auto result = decode_message(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(
      std::holds_alternative<Response>(result.value()));
}

TEST(JsonRpcCodecTest, DecodeMessageError) {
  const auto json =
      R"({"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"Method not found","data":null}})";
  auto result = decode_message(json);
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(
      std::holds_alternative<Error>(result.value()));
}

TEST(JsonRpcCodecTest, DecodeMalformedJson) {
  auto result = decode_message("{invalid}");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::InvalidJson);
}

TEST(JsonRpcCodecTest, DecodeNonObject) {
  auto result = decode_message("42");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::InvalidJson);
}

TEST(JsonRpcCodecTest, DecodeEmptyString) {
  auto result = decode_message("");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::InvalidJson);
}

TEST(JsonRpcCodecTest, EncodeResponse) {
  Response resp{.jsonrpc = "2.0",
                .result = rfl::Generic{std::string("ok")},
                .id = 1};
  auto result = encode(resp);
  ASSERT_TRUE(result.hasValue());
  EXPECT_FALSE(result.value().empty());
  EXPECT_NE(result.value().find("\"result\""), std::string::npos);
}

TEST(JsonRpcCodecTest, EncodeError) {
  Error err{.jsonrpc = "2.0",
            .id = 1,
            .error = ErrorData{.code = -32600, .message = "Bad"}};
  auto result = encode(err);
  ASSERT_TRUE(result.hasValue());
  EXPECT_NE(result.value().find("-32600"), std::string::npos);
}

TEST(JsonRpcCodecTest, EncodeNotification) {
  Notification notif{.jsonrpc = "2.0",
                     .method = "notifications/initialized"};
  auto result = encode(notif);
  ASSERT_TRUE(result.hasValue());
  EXPECT_NE(result.value().find("notifications/initialized"),
            std::string::npos);
}

TEST(JsonRpcCodecTest, EncodeRequest) {
  Request req{.jsonrpc = "2.0",
              .method = "tools/call",
              .id = 1,
              .params = rfl::Generic{std::string("{}")}};
  auto encoded = encode(req);
  ASSERT_TRUE(encoded.hasValue());
  EXPECT_NE(encoded.value().find("tools/call"), std::string::npos);
}

TEST(JsonRpcCodecTest, DecodeMessageAmbiguous) {
  auto result = decode_message(
      R"({"jsonrpc":"2.0","method":"ping","id":1,"result":{}})");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::InvalidRequest);
}

TEST(JsonRpcCodecTest, DecodeMessageAmbiguousMethodError) {
  auto result = decode_message(
      R"({"jsonrpc":"2.0","method":"ping","id":1,"error":{"code":-1}})");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code(),
            phoenix_mcp::core::ErrorCode::InvalidRequest);
}

TEST(JsonRpcCodecTest, RoundTrip) {
  const auto json =
      R"({"jsonrpc":"2.0","method":"ping","id":42})";
  auto decoded = decode_request(json);
  ASSERT_TRUE(decoded.hasValue());
  EXPECT_EQ(decoded.value().method, "ping");
  EXPECT_EQ(std::get<int>(decoded.value().id), 42);
}