#include "phoenix_mcp/transport/drogon_transport.h"

#include <arpa/inet.h>
#include <drogon/drogon.h>
#include <folly/coro/Task.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include "drogon_cancellation_state.h"

namespace phoenix_mcp::server::drogon_internal {
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

uint16_t reserve_loopback_port() {
  const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_NE(socket_fd, -1);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  EXPECT_EQ(
      ::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)),
      0);

  socklen_t address_size = sizeof(address);
  EXPECT_EQ(::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address),
                          &address_size),
            0);
  ::close(socket_fd);
  return ntohs(address.sin_port);
}

int connect_to_loopback(uint16_t port) {
  const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    return -1;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (::connect(socket_fd, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) != 0) {
    ::close(socket_fd);
    return -1;
  }
  return socket_fd;
}

TEST(DrogonCancellationStateTest, ClosingConnectionCancelsOnlyItsRequests) {
  DrogonCancellationState sources;

  const auto alpha = sources.token_for("alpha");
  const auto beta = sources.token_for("beta");

  sources.close("alpha");

  EXPECT_TRUE(alpha.isCancellationRequested());
  EXPECT_FALSE(beta.isCancellationRequested());
  EXPECT_FALSE(sources.contains("alpha"));
  EXPECT_TRUE(sources.contains("beta"));
}

TEST(DrogonTransportTest, CancelsActiveRequestWhenTcpConnectionCloses) {
  const uint16_t port = reserve_loopback_port();
  DrogonTransport transport(
      {.bind_address = "127.0.0.1", .port = port, .concurrency = 1});

  std::atomic<bool> handler_started{false};
  std::atomic<bool> handler_cancelled{false};
  std::thread server_thread([&] {
    transport.run(
        [&](ITransport::RequestEnvelope request)
            -> folly::coro::Task<std::optional<ITransport::ResponseEnvelope>> {
          handler_started = true;
          for (int i = 0; i < 400; ++i) {
            if (request.cancel_token.isCancellationRequested()) {
              handler_cancelled = true;
              break;
            }
            std::this_thread::sleep_for(5ms);
          }
          co_return ITransport::ResponseEnvelope{.body = "{}"};
        });
  });

  int client_fd = -1;
  const auto connect_deadline = std::chrono::steady_clock::now() + 2000ms;
  while (client_fd == -1 &&
         std::chrono::steady_clock::now() < connect_deadline) {
    client_fd = connect_to_loopback(port);
    if (client_fd == -1) {
      std::this_thread::sleep_for(10ms);
    }
  }

  bool wrote_request = false;
  if (client_fd != -1) {
    const std::string request =
        "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Content-Type: application/json\r\nContent-Length: 2\r\n"
        "Connection: keep-alive\r\n\r\n{}";
    wrote_request = ::write(client_fd, request.data(), request.size()) ==
                    static_cast<ssize_t>(request.size());
  }

  const bool started = wait_until(handler_started, 1000ms);
  if (client_fd != -1) {
    ::close(client_fd);
  }
  const bool cancelled = wait_until(handler_cancelled, 1000ms);

  drogon::app().quit();
  server_thread.join();

  EXPECT_TRUE(wrote_request);
  EXPECT_TRUE(started);
  EXPECT_TRUE(cancelled);
}

}  // namespace
}  // namespace phoenix_mcp::server::drogon_internal
