#include "phoenix_mcp/core/error.h"

#include <gtest/gtest.h>

using namespace phoenix_mcp::core;

TEST(McpErrorTest, OkCodeIsNotError) {
  McpError e(ErrorCode::Ok, "");
  EXPECT_FALSE(e);
  EXPECT_EQ(e.code(), ErrorCode::Ok);
}

TEST(McpErrorTest, ErrorCodeIsTruthy) {
  McpError e(ErrorCode::TransportClosed, "connection lost");
  EXPECT_TRUE(e);
  EXPECT_EQ(e.code(), ErrorCode::TransportClosed);
  EXPECT_EQ(e.message(), "connection lost");
}

TEST(McpErrorTest, CategoryTransport) {
  McpError e(ErrorCode::TransportClosed, "");
  EXPECT_EQ(e.category(), ErrorCategory::Transport);
}

TEST(McpErrorTest, CategoryProtocol) {
  McpError e(ErrorCode::InvalidRequest, "");
  EXPECT_EQ(e.category(), ErrorCategory::Protocol);
}

TEST(McpErrorTest, CategorySerialization) {
  McpError e(ErrorCode::DeserializationFailed, "");
  EXPECT_EQ(e.category(), ErrorCategory::Serialization);
}

TEST(McpErrorTest, CategoryValidation) {
  McpError e(ErrorCode::ValidationFailed, "");
  EXPECT_EQ(e.category(), ErrorCategory::Validation);
}

TEST(McpErrorTest, CategoryTimeout) {
  McpError e(ErrorCode::TimeoutExpired, "");
  EXPECT_EQ(e.category(), ErrorCategory::Timeout);
}

TEST(McpErrorTest, CategoryCancellation) {
  McpError e(ErrorCode::Cancelled, "");
  EXPECT_EQ(e.category(), ErrorCategory::Cancellation);
}

TEST(McpErrorTest, CategoryLifecycle) {
  McpError e(ErrorCode::InvalidState, "");
  EXPECT_EQ(e.category(), ErrorCategory::Lifecycle);
}

TEST(McpErrorTest, CategoryTool) {
  McpError e(ErrorCode::ToolNotFound, "");
  EXPECT_EQ(e.category(), ErrorCategory::Tool);
}

TEST(McpErrorTest, CategoryInternal) {
  McpError e(ErrorCode::InternalError, "");
  EXPECT_EQ(e.category(), ErrorCategory::Internal);
}

TEST(McpErrorTest, ErrorCodeMatchesStdErrorCode) {
  McpError e(ErrorCode::InvalidParams, "bad param");
  auto ec = e.error_code();
  EXPECT_EQ(ec.value(), static_cast<int>(ErrorCode::InvalidParams));
  EXPECT_EQ(ec.category().name(), std::string("phoenix_mcp"));
  EXPECT_FALSE(ec.message().empty());
}

TEST(McpErrorTest, MakeErrorCodeRoundTrip) {
  auto ec = make_error_code(ErrorCode::MethodNotFound);
  EXPECT_EQ(ec.value(), 202);
  EXPECT_EQ(ec.category().name(), std::string("phoenix_mcp"));
}

TEST(McpErrorTest, WithData) {
  rfl::Generic data = rfl::Generic{std::string("details")};
  McpError e(ErrorCode::InvalidParams, "missing field", data);
  EXPECT_TRUE(e);
  EXPECT_EQ(e.code(), ErrorCode::InvalidParams);
  EXPECT_EQ(e.category(), ErrorCategory::Protocol);
}

TEST(McpErrorTest, WithCause) {
  auto cause = std::make_shared<McpError>(ErrorCode::InvalidJson, "bad JSON");
  McpError e(ErrorCode::DeserializationFailed, "parse failed", cause);
  EXPECT_TRUE(e);
  ASSERT_NE(e.cause(), nullptr);
  EXPECT_EQ(e.cause()->code(), ErrorCode::InvalidJson);
}

TEST(McpErrorTest, ToString) {
  McpError e(ErrorCode::ToolNotFound, "no such tool");
  std::string s = e.to_string();
  EXPECT_FALSE(s.empty());
  EXPECT_NE(s.find("not found"), std::string::npos);
}

TEST(McpErrorTest, ToStringWithCause) {
  auto cause = std::make_shared<McpError>(ErrorCode::InvalidJson, "bad JSON");
  McpError e(ErrorCode::DeserializationFailed, "parse failed", cause);
  std::string s = e.to_string();
  EXPECT_NE(s.find("caused by"), std::string::npos);
}

TEST(McpErrorTest, DefaultConstructorIsOk) {
  McpError e;
  EXPECT_FALSE(e);
  EXPECT_EQ(e.code(), ErrorCode::Ok);
}

TEST(McpErrorTest, ErrorMessageCoverage) {
  for (int i = 0; i <= static_cast<int>(ErrorCode::InternalError); ++i) {
    auto code = static_cast<ErrorCode>(i);
    auto ec = make_error_code(code);
    EXPECT_FALSE(ec.message().empty())
        << "Empty message for code " << i;
  }
}