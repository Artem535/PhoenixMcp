#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/server/server.h"
#include "phoenix_mcp/transport/stdio_transport.h"

int main() {
  phoenix_mcp::msg::types::Implementation info{
    .name = "consumer-test", .version = "1.0.0"};
  return 0;
}