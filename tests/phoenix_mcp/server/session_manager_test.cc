#include "phoenix_mcp/server/session_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include <folly/coro/BlockingWait.h>

#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

SessionManager::SessionFactory make_factory() {
  return [] {
    ServerCapabilities capabilities{
        .tools = ToolsCapabilities{.list_changed = false}};
    Implementation info{.name = "test", .version = "0.0.0"};
    return std::make_unique<ServerSession>(
        capabilities, info, "instruction",
        std::make_unique<phoenix_mcp::tool::ToolRegistry>());
  };
}

std::unique_ptr<SessionManager> make_manager(
    std::chrono::milliseconds idle_timeout = std::chrono::milliseconds(0),
    size_t max_sessions = 0) {
  return std::make_unique<SessionManager>(make_factory(), idle_timeout,
                                          max_sessions);
}

}  // namespace

TEST(SessionManagerTest, CreateSessionReturnsDistinctSessionsPerConnection) {
  const auto manager = make_manager();
  auto session_a = manager->create_session("conn-a");
  auto session_b = manager->create_session("conn-b");

  ASSERT_NE(session_a, nullptr);
  ASSERT_NE(session_b, nullptr);
  EXPECT_NE(session_a, session_b);
  EXPECT_EQ(manager->session_count(), 2u);

  session_a->handle_input(
      R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
      R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
      R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})");
  EXPECT_EQ(session_a->state_name(), "Initializing");
  EXPECT_EQ(session_b->state_name(), "Uninitialized");
}

TEST(SessionManagerTest, CreateSessionRejectsDuplicateConnectionId) {
  const auto manager = make_manager();
  ASSERT_NE(manager->create_session("conn-a"), nullptr);
  EXPECT_EQ(manager->create_session("conn-a"), nullptr);
  EXPECT_EQ(manager->session_count(), 1u);
}

TEST(SessionManagerTest, CreateSessionEnforcesMaxSessions) {
  const auto manager = make_manager(std::chrono::milliseconds(0), 1);

  ASSERT_NE(manager->create_session("conn-a"), nullptr);
  EXPECT_EQ(manager->create_session("conn-b"), nullptr);
  EXPECT_EQ(manager->session_count(), 1u);
}

TEST(SessionManagerTest, GetSessionReturnsNullForUnknownConnection) {
  const auto manager = make_manager();
  EXPECT_EQ(manager->get_session("does-not-exist"), nullptr);
}

TEST(SessionManagerTest, RemoveSessionClosesAndForgetsSession) {
  const auto manager = make_manager();
  ASSERT_NE(manager->create_session("conn-a"), nullptr);

  manager->remove_session("conn-a");

  EXPECT_EQ(manager->get_session("conn-a"), nullptr);
  EXPECT_EQ(manager->session_count(), 0u);
}

TEST(SessionManagerTest, RemoveSessionOfUnknownConnectionIsNoOp) {
  const auto manager = make_manager();
  manager->remove_session("does-not-exist");
  EXPECT_EQ(manager->session_count(), 0u);
}

TEST(SessionManagerTest, ExpireIdleSessionsIsNoOpWhenIdleTimeoutIsZero) {
  const auto manager = make_manager();  // idle_timeout defaults to 0.
  ASSERT_NE(manager->create_session("conn-a"), nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  manager->expire_idle_sessions();

  EXPECT_NE(manager->get_session("conn-a"), nullptr);
}

TEST(SessionManagerTest, ExpireIdleSessionsRemovesSessionsPastIdleTimeout) {
  const auto manager = make_manager(std::chrono::milliseconds(20));
  ASSERT_NE(manager->create_session("conn-a"), nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  manager->expire_idle_sessions();

  EXPECT_EQ(manager->get_session("conn-a"), nullptr);
  EXPECT_EQ(manager->session_count(), 0u);
}

TEST(SessionManagerTest, TouchSessionResetsIdleTimerSoExpireIdleSessionsKeepsIt) {
  const auto manager = make_manager(std::chrono::milliseconds(30));
  ASSERT_NE(manager->create_session("conn-a"), nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  manager->touch_session("conn-a");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  manager->expire_idle_sessions();

  EXPECT_NE(manager->get_session("conn-a"), nullptr)
      << "touch_session should have reset the idle timer";
}

TEST(SessionManagerTest, ShutdownAllClosesAndRemovesEverySession) {
  const auto manager = make_manager();
  ASSERT_NE(manager->create_session("conn-a"), nullptr);
  ASSERT_NE(manager->create_session("conn-b"), nullptr);

  folly::coro::blockingWait(manager->shutdown_all());

  EXPECT_EQ(manager->session_count(), 0u);
  EXPECT_EQ(manager->get_session("conn-a"), nullptr);
  EXPECT_EQ(manager->get_session("conn-b"), nullptr);
}

TEST(SessionManagerTest, CreateSessionRefusesNewSessionsAfterShutdownAll) {
  const auto manager = make_manager();
  ASSERT_NE(manager->create_session("conn-a"), nullptr);

  folly::coro::blockingWait(manager->shutdown_all());

  EXPECT_EQ(manager->create_session("conn-b"), nullptr);
  EXPECT_EQ(manager->session_count(), 0u);
}
