//
// Created by artem.d on 09.11.2025.
//

#include "stdio_transport.h"

#if defined(_WIN32)
#include <iostream>
#else
#include <poll.h>
#include <unistd.h>
#endif

#include <folly/CancellationToken.h>
#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <optional>
#include <thread>

namespace phoenix_mcp::server {

namespace {

#if !defined(_WIN32)
std::optional<std::string> read_line(int fd) {
  std::string line;
  char character;
  while (true) {
    const ssize_t read_count = ::read(fd, &character, 1);
    if (read_count == 0) {
      return std::nullopt;
    }
    if (read_count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return std::nullopt;
    }
    if (character == '\n') {
      return line;
    }
    line.push_back(character);
  }
}

void write_line(int fd, const std::string& body) {
  const std::string line = body + "\n";
  size_t written = 0;
  while (written < line.size()) {
    const ssize_t write_count =
        ::write(fd, line.data() + written, line.size() - written);
    if (write_count < 0) {
      if (errno == EINTR) {
        continue;
      }
      spdlog::error("StdioTransport| write failed: {}", std::strerror(errno));
      return;
    }
    written += static_cast<size_t>(write_count);
  }
}

void watch_for_disconnect(int fd, folly::CancellationSource& source,
                          const std::atomic<bool>& stop) {
  while (!stop.load(std::memory_order_relaxed)) {
    pollfd descriptor{fd, 0, 0};
    const int result = ::poll(&descriptor, 1, 100);
    if (result > 0 && (descriptor.revents & (POLLHUP | POLLERR))) {
      source.requestCancellation();
      return;
    }
  }
}
#endif

}  // namespace

#if defined(_WIN32)
StdioTransport::StdioTransport() = default;
#else
StdioTransport::StdioTransport()
    : StdioTransport(STDIN_FILENO, STDOUT_FILENO) {}
#endif

StdioTransport::StdioTransport(int input_fd, int output_fd)
    : input_fd_(input_fd), output_fd_(output_fd) {}

int StdioTransport::run(Handler on_message) {
#if defined(_WIN32)
  std::string line;
  while (true) {
    if (!std::getline(std::cin, line)) {
      spdlog::info("StdioTransport| stdin closed -> shutdown");
      return 0;
    }

    if (line.empty()) {
      spdlog::info("StdioTransport| empty line -> shutdown");
      return 0;
    }

    ITransport::RequestEnvelope request;
    request.body = std::move(line);
    // A process only ever owns one stdio channel, so every request here
    // belongs to that single implicit connection. Leaving connection_id
    // empty routes all of them to the same session via SessionManager's
    // shared-default-connection handling (McpRequestHandler) — used as a
    // single-entry manager, not bypassed — rather than introducing a
    // stdio-specific code path.
    const auto response =
        folly::coro::blockingWait(on_message(std::move(request)));
    if (!response.has_value()) {
      continue;
    }

    if (response->body == "null") {
      spdlog::error("StdioTransport| refusing to write 'null' to stdout");
      continue;
    }

    std::cout << response->body << '\n';
    std::cout.flush();
    line.clear();
  }
#else
  folly::CancellationSource disconnect_source;
  std::atomic<bool> stop_watching{false};
  std::thread watcher([&] {
    watch_for_disconnect(input_fd_, disconnect_source, stop_watching);
  });

  while (true) {
    auto line = read_line(input_fd_);
    if (!line.has_value()) {
      spdlog::info("StdioTransport| stdin closed -> shutdown");
      break;
    }

    if (line->empty()) {
      spdlog::info("StdioTransport| empty line -> shutdown");
      break;
    }

    ITransport::RequestEnvelope request;
    request.body = std::move(*line);
    request.cancel_token = disconnect_source.getToken();
    const auto response =
        folly::coro::blockingWait(on_message(std::move(request)));
    if (!response.has_value()) {
      continue;
    }

    if (response->body == "null") {
      spdlog::error("StdioTransport| refusing to write 'null' to stdout");
      continue;
    }

    write_line(output_fd_, response->body);
  }

  stop_watching.store(true, std::memory_order_relaxed);
  watcher.join();
  return 0;
#endif
}
}  // namespace phoenix_mcp::server
