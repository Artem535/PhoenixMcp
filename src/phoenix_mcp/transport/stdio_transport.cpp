//
// Created by artem.d on 09.11.2025.
//

#include "stdio_transport.h"

#include <poll.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <thread>

#include <folly/CancellationToken.h>
#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

namespace phoenix_mcp::server {

namespace {

// Reads one newline-terminated line from `fd`, byte at a time (simple over
// fast: this is one JSON-RPC message per line on a local pipe, not a
// throughput-sensitive path). Returns std::nullopt on EOF or a read error.
std::optional<std::string> read_line(int fd) {
  std::string line;
  char ch;
  while (true) {
    const ssize_t n = ::read(fd, &ch, 1);
    if (n == 0) return std::nullopt;
    if (n < 0) {
      if (errno == EINTR) continue;
      return std::nullopt;
    }
    if (ch == '\n') return line;
    line.push_back(ch);
  }
}

void write_line(int fd, const std::string& body) {
  const std::string line = body + "\n";
  size_t written = 0;
  while (written < line.size()) {
    const ssize_t n =
        ::write(fd, line.data() + written, line.size() - written);
    if (n < 0) {
      if (errno == EINTR) continue;
      spdlog::error("StdioTransport| write failed: {}", strerror(errno));
      return;
    }
    written += static_cast<size_t>(n);
  }
}

// Runs on a background thread for the transport's whole lifetime, polling
// `fd` for POLLHUP/POLLERR so a client closing its end of the pipe cancels
// whatever request is in flight -- including one the main thread is
// blocked inside a handler for, not just one it's about to read. Polling
// (not reading) `fd` from a second thread while the main thread reads it is
// safe on POSIX; only actual reads need to be single-threaded.
void watch_for_hangup(int fd, folly::CancellationSource& source,
                      const std::atomic<bool>& stop) {
  while (!stop.load(std::memory_order_relaxed)) {
    pollfd pfd{fd, POLLIN, 0};
    const int rv = ::poll(&pfd, 1, 100);
    if (rv > 0 && (pfd.revents & (POLLHUP | POLLERR))) {
      source.requestCancellation();
      return;
    }
  }
}

}  // namespace

StdioTransport::StdioTransport() : StdioTransport(STDIN_FILENO, STDOUT_FILENO) {}

StdioTransport::StdioTransport(int input_fd, int output_fd)
    : input_fd_(input_fd), output_fd_(output_fd) {}

int StdioTransport::run(Handler on_message) {
  folly::CancellationSource disconnect_source;
  std::atomic<bool> stop_watching{false};
  std::thread watcher(
      [&] { watch_for_hangup(input_fd_, disconnect_source, stop_watching); });

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
    // A process only ever owns one stdio channel, so every request here
    // belongs to that single implicit connection. Leaving connection_id
    // empty routes all of them to the same session via SessionManager's
    // shared-default-connection handling (McpRequestHandler) — used as a
    // single-entry manager, not bypassed — rather than introducing a
    // stdio-specific code path.
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
}

}  // namespace phoenix_mcp::server
