#include "phoenix_mcp/transport/stdio_transport.h"

#include <folly/coro/Task.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

namespace phoenix_mcp::server {
namespace {

using namespace std::chrono_literals;

bool wait_until(const std::atomic<bool>& value,
                std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!value.load(std::memory_order_relaxed) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
  }
  return value.load(std::memory_order_relaxed);
}

TEST(StdioTransportTest, CancelsActiveRequestWhenInputPipeCloses) {
  int input_pipe[2];
  int output_pipe[2];
  ASSERT_EQ(::pipe(input_pipe), 0);
  ASSERT_EQ(::pipe(output_pipe), 0);

  std::atomic<bool> handler_started{false};
  std::atomic<bool> handler_cancelled{false};
  std::atomic<bool> handler_finished{false};

  StdioTransport transport(input_pipe[0], output_pipe[1]);
  ITransport::Handler handler = [&](ITransport::RequestEnvelope request)
      -> folly::coro::Task<std::optional<ITransport::ResponseEnvelope>> {
    handler_started = true;
    for (int i = 0; i < 400; ++i) {
      if (request.cancel_token.isCancellationRequested()) {
        handler_cancelled = true;
        break;
      }
      std::this_thread::sleep_for(5ms);
    }
    handler_finished = true;
    co_return ITransport::ResponseEnvelope{.body = "{}"};
  };

  std::thread runner([&] { transport.run(std::move(handler)); });

  const std::string request = "{\"jsonrpc\":\"2.0\"}\n";
  ASSERT_EQ(::write(input_pipe[1], request.data(), request.size()),
            static_cast<ssize_t>(request.size()));

  const bool started = wait_until(handler_started, 1000ms);
  ::close(input_pipe[1]);
  const bool cancelled = wait_until(handler_cancelled, 1000ms);
  const bool finished = wait_until(handler_finished, 2000ms);
  runner.join();

  ::close(input_pipe[0]);
  ::close(output_pipe[0]);
  ::close(output_pipe[1]);

  EXPECT_TRUE(started);
  EXPECT_TRUE(cancelled);
  EXPECT_TRUE(finished);
}

}  // namespace
}  // namespace phoenix_mcp::server
