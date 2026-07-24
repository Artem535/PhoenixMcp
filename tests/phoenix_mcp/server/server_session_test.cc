#include "phoenix_mcp/server/server_session.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

std::unique_ptr<ServerSession> make_session(
    ServerConfig config = {},
    std::unique_ptr<phoenix_mcp::tool::ToolRegistry> tool_registry = nullptr) {
  ServerCapabilities capabilities{
      .tools = ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  if (!tool_registry) {
    tool_registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  }
  return std::make_unique<ServerSession>(capabilities, info, "instruction",
                                         std::move(tool_registry), config);
}

constexpr auto kInitializeRequest =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
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

namespace {

struct WaitToolInput {
  int poll_iterations = 200;
};

}  // namespace

TEST(ServerSessionTest, NotificationsCancelledStopsInFlightTool) {
  std::atomic<bool> tool_started{false};
  std::atomic<bool> tool_saw_cancellation{false};

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitToolInput>(
      "wait_tool", "Waits until cancelled or polling runs out",
      [&](const WaitToolInput& input, const folly::CancellationToken& token)
          -> folly::coro::Task<CallToolResult> {
        tool_started = true;
        for (int i = 0; i < input.poll_iterations; ++i) {
          if (token.isCancellationRequested()) {
            tool_saw_cancellation = true;
            co_return CallToolResult{
                .content = {TextContent{.text = "cancelled"}},
                .is_error = true};
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        co_return CallToolResult{.content = {TextContent{.text = "timed out"}},
                                 .is_error = true};
      });

  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  std::optional<rfl::Generic> call_result;
  std::thread worker([&] {
    call_result = session->handle_input(
        R"({"jsonrpc":"2.0","method":"tools/call","id":42,)"
        R"("params":{"name":"wait_tool","arguments":{"poll_iterations":200}}})");
  });

  for (int i = 0; i < 200 && !tool_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!tool_started) {
    worker.join();
    FAIL() << "tool never started polling";
  }

  session->handle_input(
      R"({"jsonrpc":"2.0","method":"notifications/cancelled",)"
      R"("params":{"requestId":42}})");

  worker.join();

  EXPECT_TRUE(tool_saw_cancellation);
  ASSERT_TRUE(call_result.has_value());
  const auto response_json = rfl::json::write(*call_result);
  EXPECT_NE(response_json.find("cancelled"), std::string::npos);
}

TEST(ServerSessionTest, TransportCancellationStopsInFlightTool) {
  std::atomic<bool> tool_started{false};
  std::atomic<bool> tool_saw_cancellation{false};

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitToolInput>(
      "wait_tool", "Waits until cancelled or polling runs out",
      [&](const WaitToolInput& input, const folly::CancellationToken& token)
          -> folly::coro::Task<CallToolResult> {
        tool_started = true;
        for (int i = 0; i < input.poll_iterations; ++i) {
          if (token.isCancellationRequested()) {
            tool_saw_cancellation = true;
            co_return CallToolResult{
                .content = {TextContent{.text = "cancelled"}},
                .is_error = true};
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        co_return CallToolResult{.content = {TextContent{.text = "timed out"}},
                                 .is_error = true};
      });

  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  // A real transport (RequestEnvelope::cancel_token) would derive this from
  // its own disconnect detection (closed stdio pipe, dropped HTTP
  // connection); this test drives that same parameter directly to verify
  // ServerSession merges it with protocol-level cancellation correctly,
  // without needing a real transport.
  folly::CancellationSource transport_source;

  std::optional<rfl::Generic> call_result;
  std::thread worker([&] {
    call_result = session->handle_input(
        R"({"jsonrpc":"2.0","method":"tools/call","id":43,)"
        R"("params":{"name":"wait_tool","arguments":{"poll_iterations":200}}})",
        transport_source.getToken());
  });

  for (int i = 0; i < 200 && !tool_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!tool_started) {
    worker.join();
    FAIL() << "tool never started polling";
  }

  transport_source.requestCancellation();

  worker.join();

  EXPECT_TRUE(tool_saw_cancellation);
  ASSERT_TRUE(call_result.has_value());
  const auto response_json = rfl::json::write(*call_result);
  EXPECT_NE(response_json.find("cancelled"), std::string::npos);
}

TEST(ServerSessionTest, ConcurrentPingIsNotBlockedByInFlightToolCall) {
  std::atomic<bool> tool_started{false};
  std::atomic<bool> release_tool{false};

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitToolInput>(
      "wait_tool", "Blocks until released",
      [&](const WaitToolInput&, const folly::CancellationToken&)
          -> folly::coro::Task<CallToolResult> {
        tool_started = true;
        while (!release_tool) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        co_return CallToolResult{.content = {TextContent{.text = "done"}},
                                 .is_error = false};
      });

  const auto session = make_session({}, std::move(registry));
  initialize(*session);

  std::thread worker([&] {
    session->handle_input(
        R"({"jsonrpc":"2.0","method":"tools/call","id":50,)"
        R"("params":{"name":"wait_tool","arguments":{"poll_iterations":0}}})");
  });

  for (int i = 0; i < 200 && !tool_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!tool_started) {
    release_tool = true;
    worker.join();
    FAIL() << "tool never started";
  }

  // If fsm_mutex_ were (incorrectly) held across the in-flight tool call's
  // co_await, this ping would block until the tool above is released. Run
  // it on its own thread with a bounded wait so a regression fails fast
  // instead of hanging the test suite.
  std::promise<std::optional<rfl::Generic>> ping_promise;
  auto ping_future = ping_promise.get_future();
  std::thread ping_thread([&] {
    ping_promise.set_value(
        session->handle_input(R"({"jsonrpc":"2.0","method":"ping","id":51})"));
  });

  const auto ping_status = ping_future.wait_for(std::chrono::milliseconds(500));
  release_tool = true;
  worker.join();
  ping_thread.join();

  ASSERT_EQ(ping_status, std::future_status::ready)
      << "ping was blocked behind the in-flight tool call";
  ASSERT_TRUE(ping_future.get().has_value());
}

TEST(ServerSessionTest, CloseWaitsForInFlightToolToDrainThenSettles) {
  std::atomic<bool> tool_started{false};
  std::atomic<bool> tool_saw_cancellation{false};

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitToolInput>(
      "wait_tool", "Waits until cancelled or polling runs out",
      [&](const WaitToolInput& input, const folly::CancellationToken& token)
          -> folly::coro::Task<CallToolResult> {
        tool_started = true;
        for (int i = 0; i < input.poll_iterations; ++i) {
          if (token.isCancellationRequested()) {
            tool_saw_cancellation = true;
            co_return CallToolResult{
                .content = {TextContent{.text = "cancelled"}},
                .is_error = true};
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        co_return CallToolResult{.content = {TextContent{.text = "timed out"}},
                                 .is_error = true};
      });

  ServerConfig config;
  config.settle_timeout = std::chrono::milliseconds(2000);
  const auto session = make_session(config, std::move(registry));
  initialize(*session);

  std::thread worker([&] {
    session->handle_input(
        R"({"jsonrpc":"2.0","method":"tools/call","id":60,)"
        R"("params":{"name":"wait_tool","arguments":{"poll_iterations":200}}})");
  });

  for (int i = 0; i < 200 && !tool_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!tool_started) {
    worker.join();
    FAIL() << "tool never started";
  }

  // close() should signal cancellation to the in-flight tool call and return
  // once it drains, well before the (generous) 2s settle timeout.
  const auto started_at = std::chrono::steady_clock::now();
  session->close();
  const auto elapsed = std::chrono::steady_clock::now() - started_at;

  worker.join();

  EXPECT_EQ(session->state_name(), "Settled");
  EXPECT_TRUE(tool_saw_cancellation);
  EXPECT_LT(elapsed, std::chrono::milliseconds(1000));
}

TEST(ServerSessionTest, RejectsMalformedJsonWithCodecErrorCode) {
  const auto session = make_session();
  const auto response = session->handle_input("not valid json");
  ASSERT_TRUE(response.has_value());
  const auto response_json = rfl::json::write(*response);
  // -32700 is JsonRpcCodec's ErrorCode::InvalidJson mapped to its JSON-RPC
  // wire code, not a session-invented one.
  EXPECT_NE(response_json.find("-32700"), std::string::npos) << response_json;
}

TEST(ServerSessionTest, InitializeWithUnsupportedProtocolVersionFailsInitialization) {
  const auto session = make_session();
  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
      R"("params":{"protocolVersion":"1999-01-01","capabilities":{},)"
      R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})");
  ASSERT_TRUE(response.has_value());
  const auto response_json = rfl::json::write(*response);
  // -32600 is JsonRpcCodec's ErrorCode::InvalidRequest mapped to its
  // JSON-RPC wire code — negotiation failure is a Protocol-category error,
  // not a session-invented one.
  EXPECT_NE(response_json.find("-32600"), std::string::npos) << response_json;
  EXPECT_EQ(session->state_name(), "Failed");
}

TEST(ServerSessionTest, CloseForceSettlesAfterSettleTimeoutElapses) {
  std::atomic<bool> tool_started{false};

  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_cancellable_tool<WaitToolInput>(
      "stubborn_tool", "Ignores cancellation for a fixed duration",
      [&](const WaitToolInput& input, const folly::CancellationToken&)
          -> folly::coro::Task<CallToolResult> {
        tool_started = true;
        for (int i = 0; i < input.poll_iterations; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        co_return CallToolResult{.content = {TextContent{.text = "finished"}},
                                 .is_error = false};
      });

  ServerConfig config;
  config.settle_timeout = std::chrono::milliseconds(50);
  const auto session = make_session(config, std::move(registry));
  initialize(*session);

  std::thread worker([&] {
    session->handle_input(
        R"({"jsonrpc":"2.0","method":"tools/call","id":61,)"
        R"("params":{"name":"stubborn_tool","arguments":{"poll_iterations":40}}})");
  });

  for (int i = 0; i < 200 && !tool_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!tool_started) {
    worker.join();
    FAIL() << "tool never started";
  }

  // The tool ignores cancellation and runs for ~200ms; close() must not wait
  // that long — it force-settles once the 50ms settle deadline passes.
  session->close();
  EXPECT_EQ(session->state_name(), "Settled")
      << "close() should force-settle once the settle deadline passes, even "
         "with a tool still running";

  worker.join();
}
