#ifndef PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_

#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class StdioTransport final : public ITransport {
 public:
  StdioTransport();
  StdioTransport(int input_fd, int output_fd);

  int run(Handler on_message) override;

 private:
  int input_fd_ = -1;
  int output_fd_ = -1;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
