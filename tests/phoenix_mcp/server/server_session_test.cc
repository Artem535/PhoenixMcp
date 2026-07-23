#include "phoenix_mcp/server/server_session.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

std::unique_ptr<ServerSession> make_session(ServerConfig config = {}) {
  ServerCapabilities capabilities{
      .tools = ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  return std::make_unique<ServerSession>(
      capabilities, info, "instruction",
      std::make_unique<phoenix_mcp::tool::ToolRegistry>(), config);
}

constexpr auto kInitializeRequest =
    R"({"jsonrpc":"2.0","method":"initialize","id":1})";
constexpr auto kInitializedNotification =
    R"({"jsonrpc":"2.0","method":"notifications/initialized"})";

void initialize(ServerSession& session) {
  session.handle_input(kInitializeRequest);
  session.handle_input(kInitializedNotification);
}

}  // namespace

TEST(ServerSessionTest, StartsUninitialized) {
  const auto session = make_session();
  EXPECT_EQ(session->state_name(), "Uninitialized");
  EXPECT_FALSE(session->is_ready());
}

TEST(ServerSessionTest, RejectsNonInitializeRequestWhileUninitialized) {
  const auto session = make_session();
  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"ping","id":1})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Uninitialized");
}

TEST(ServerSessionTest, InitializeRequestMovesToInitializing) {
  const auto session = make_session();
  const auto response = session->handle_input(kInitializeRequest);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Initializing");
  EXPECT_FALSE(session->is_ready());
}

TEST(ServerSessionTest, RejectsRequestsWhileInitializing) {
  const auto session = make_session();
  session->handle_input(kInitializeRequest);
  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"ping","id":2})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Initializing");
}

TEST(ServerSessionTest, InitializedNotificationMovesToOperation) {
  const auto session = make_session();
  initialize(*session);
  EXPECT_EQ(session->state_name(), "Operation");
  EXPECT_TRUE(session->is_ready());
}

TEST(ServerSessionTest, RepeatedInitializeRequestIsAllowedInOperation) {
  const auto session = make_session();
  initialize(*session);
  const auto response = session->handle_input(kInitializeRequest);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Operation");
}

TEST(ServerSessionTest, PingWorksInOperation) {
  const auto session = make_session();
  initialize(*session);
  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"ping","id":3})");
  ASSERT_TRUE(response.has_value());
}

TEST(ServerSessionTest, UnknownMethodReturnsErrorInOperation) {
  const auto session = make_session();
  initialize(*session);
  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"nonexistent","id":4})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Operation");
}

TEST(ServerSessionTest, CloseFromOperationSettles) {
  const auto session = make_session();
  initialize(*session);
  session->close();
  EXPECT_EQ(session->state_name(), "Settled");
  EXPECT_FALSE(session->is_ready());
}

TEST(ServerSessionTest, CloseIsIdempotent) {
  const auto session = make_session();
  initialize(*session);
  session->close();
  session->close();
  EXPECT_EQ(session->state_name(), "Settled");
}

TEST(ServerSessionTest, RejectsRequestsAfterClose) {
  const auto session = make_session();
  initialize(*session);
  session->close();
  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"ping","id":5})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Settled");
}

TEST(ServerSessionTest, InitializationTimeoutMovesToFailed) {
  ServerConfig config;
  config.init_timeout = std::chrono::milliseconds(1);
  const auto session = make_session(config);

  session->handle_input(kInitializeRequest);
  ASSERT_EQ(session->state_name(), "Initializing");

  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"ping","id":6})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(session->state_name(), "Failed");
}

TEST(ServerSessionTest, CloseFromFailedSettles) {
  ServerConfig config;
  config.init_timeout = std::chrono::milliseconds(1);
  const auto session = make_session(config);

  session->handle_input(kInitializeRequest);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  session->handle_input(R"({"jsonrpc":"2.0","method":"ping","id":7})");
  ASSERT_EQ(session->state_name(), "Failed");

  session->close();
  EXPECT_EQ(session->state_name(), "Settled");
}
