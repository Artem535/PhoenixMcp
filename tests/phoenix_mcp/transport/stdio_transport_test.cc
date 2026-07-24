#include "phoenix_mcp/transport/stdio_transport.h"

#include <unistd.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include <folly/coro/Task.h>

using namespace phoenix_mcp::server;

TEST(StdioTransportTest, DisconnectCancelsInFlightHandler) {
  int in_fds[2];
  ASSERT_EQ(::pipe(in_fds), 0);
  int out_fds[2];
  ASSERT_EQ(::pipe(out_fds), 0);

  StdioTransport transport(in_fds[0], out_fds[1]);

  std::atomic<bool> handler_started{false};
  std::atomic<bool> handler_saw_cancellation{false};

  ITransport::Handler handler =
      [&](ITransport::RequestEnvelope request)
          -> folly::coro::Task<std::optional<ITransport::ResponseEnvelope>> {
    handler_started = true;
    for (int i = 0; i < 200; ++i) {
      if (request.cancel_token.isCancellationRequested()) {
        handler_saw_cancellation = true;
        co_return ITransport::ResponseEnvelope{.body = "{}"};
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    co_return ITransport::ResponseEnvelope{.body = "{}"};
  };

  std::thread transport_thread([&] { transport.run(handler); });

  const std::string request_line =
      R"({"jsonrpc":"2.0","method":"ping","id":1})"
      "\n";
  ASSERT_EQ(::write(in_fds[1], request_line.data(), request_line.size()),
           static_cast<ssize_t>(request_line.size()));

  for (int i = 0; i < 200 && !handler_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_TRUE(handler_started) << "handler never started";

  // Simulates the client disconnecting mid-request.
  ::close(in_fds[1]);

  for (int i = 0; i < 200 && !handler_saw_cancellation; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_TRUE(handler_saw_cancellation)
      << "handler never observed transport-level cancellation";

  transport_thread.join();
  ::close(in_fds[0]);
  ::close(out_fds[0]);
  ::close(out_fds[1]);
}
