#ifndef PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_

#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class StdioTransport final : public ITransport {
 public:
  int run(Handler on_message) override;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_STDIO_TRANSPORT_H_
