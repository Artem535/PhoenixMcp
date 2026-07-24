#ifndef PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_

#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class StdioTransport final : public ITransport {
 public:
  StdioTransport();

  /// @brief Constructs a transport reading/writing raw file descriptors
  ///        instead of std::cin/std::cout.
  /// @param input_fd Read end of the input stream (defaults to stdin).
  /// @param output_fd Write end of the output stream (defaults to stdout).
  ///
  /// Exists so tests can drive the transport over a real pipe (to simulate a
  /// client disconnecting mid-request) without touching the process's real
  /// stdin/stdout; production code should just use the default constructor.
  StdioTransport(int input_fd, int output_fd);

  int run(Handler on_message) override;

 private:
  int input_fd_;
  int output_fd_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
