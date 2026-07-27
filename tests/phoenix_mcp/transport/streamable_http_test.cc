#include <arpa/inet.h>
#include <drogon/drogon.h>
#include <folly/coro/BlockingWait.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "phoenix_mcp/server/server_message_sink.h"
#include "phoenix_mcp/transport/drogon_transport.h"

namespace phoenix_mcp::server {
namespace {

using namespace std::chrono_literals;

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
  if (socket_fd == -1) return -1;
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

bool write_all(int socket_fd, const std::string& request) {
  return ::write(socket_fd, request.data(), request.size()) ==
         static_cast<ssize_t>(request.size());
}

std::string read_until(int socket_fd, std::string_view marker,
                       std::chrono::milliseconds timeout = 2s) {
  std::string result;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline &&
         result.find(marker) == std::string::npos) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - std::chrono::steady_clock::now());
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(socket_fd, &readable);
    timeval wait{.tv_sec = static_cast<long>(remaining.count() / 1000000),
                 .tv_usec = static_cast<long>(remaining.count() % 1000000)};
    if (::select(socket_fd + 1, &readable, nullptr, nullptr, &wait) <= 0) break;
    char buffer[1024];
    const auto read_size = ::read(socket_fd, buffer, sizeof(buffer));
    if (read_size <= 0) break;
    result.append(buffer, static_cast<size_t>(read_size));
  }
  return result;
}

std::string header_value(const std::string& response, std::string_view name) {
  const std::string prefix = std::string(name) + ": ";
  const auto start = response.find(prefix);
  if (start == std::string::npos) return {};
  const auto value_start = start + prefix.size();
  const auto value_end = response.find("\r\n", value_start);
  return response.substr(value_start, value_end - value_start);
}

std::string event_id(const std::string& event) {
  const auto start = event.find("id: ");
  if (start == std::string::npos) return {};
  const auto value_start = start + 4;
  const auto value_end = event.find('\n', value_start);
  return event.substr(value_start, value_end - value_start);
}

TEST(StreamableHttpTest, DeliversAndReplaysServerMessageOverSse) {
  const uint16_t port = reserve_loopback_port();
  DrogonTransport transport(
      {.bind_address = "127.0.0.1", .port = port, .concurrency = 1});
  std::mutex session_mutex;
  std::string session_id;
  std::thread server_thread([&] {
    transport.run(
        [&](ITransport::RequestEnvelope request)
            -> folly::coro::Task<std::optional<ITransport::ResponseEnvelope>> {
          if (request.session_lookup_mode ==
              ITransport::SessionLookupMode::Bootstrap) {
            std::lock_guard lock(session_mutex);
            session_id = request.connection_id;
          }
          co_return ITransport::ResponseEnvelope{.body = "{}"};
        });
  });

  int init_socket = -1;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (init_socket == -1 && std::chrono::steady_clock::now() < deadline) {
    init_socket = connect_to_loopback(port);
    if (init_socket == -1) std::this_thread::sleep_for(10ms);
  }
  ASSERT_NE(init_socket, -1);
  const std::string init_body = "{}";
  ASSERT_TRUE(
      write_all(init_socket,
                "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                "Content-Type: application/json\r\nContent-Length: 2\r\n"
                "Connection: close\r\n\r\n" +
                    init_body));
  const auto init_response = read_until(init_socket, "\r\n\r\n");
  ::close(init_socket);
  const auto issued_session_id = header_value(init_response, "mcp-session-id");
  ASSERT_FALSE(issued_session_id.empty());

  int first_stream = connect_to_loopback(port);
  ASSERT_NE(first_stream, -1);
  ASSERT_TRUE(write_all(first_stream,
                        "GET /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                        "Accept: text/event-stream\r\nMcp-Session-Id: " +
                            issued_session_id + "\r\n\r\n"));
  ASSERT_NE(read_until(first_stream, "retry: 1000").find("retry: 1000"),
            std::string::npos);

  std::string handler_session_id;
  {
    std::lock_guard lock(session_mutex);
    handler_session_id = session_id;
  }
  ASSERT_EQ(handler_session_id, issued_session_id);
  ASSERT_TRUE(folly::coro::blockingWait(transport.message_sink()->publish(
      handler_session_id, R"({"method":"notifications/message-one"})")));
  const auto first_event = read_until(first_stream, "message-one");
  ASSERT_NE(first_event.find("message-one"), std::string::npos);
  const auto first_event_id = event_id(first_event);
  ASSERT_FALSE(first_event_id.empty());
  ::close(first_stream);

  folly::coro::blockingWait(transport.message_sink()->publish(
      handler_session_id, R"({"method":"notifications/message-two"})"));

  int resumed_stream = connect_to_loopback(port);
  ASSERT_NE(resumed_stream, -1);
  ASSERT_TRUE(write_all(resumed_stream,
                        "GET /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                        "Accept: text/event-stream\r\nMcp-Session-Id: " +
                            issued_session_id + "\r\nLast-Event-ID: " +
                            first_event_id + "\r\n\r\n"));
  const auto replay = read_until(resumed_stream, "message-two");
  ::close(resumed_stream);

  drogon::app().quit();
  server_thread.join();
  EXPECT_NE(replay.find("message-two"), std::string::npos);
  EXPECT_EQ(replay.find("message-one"), std::string::npos);
}

}  // namespace
}  // namespace phoenix_mcp::server
