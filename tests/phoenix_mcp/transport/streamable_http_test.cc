#include <gtest/gtest.h>

#include "phoenix_mcp/transport/drogon_transport.h"

namespace phoenix_mcp::server {
namespace {

TEST(StreamableHttpTest, DrogonTransportProvidesSessionMessageSink) {
  DrogonTransport transport;

  EXPECT_NE(transport.message_sink(), nullptr);
}

}  // namespace
}  // namespace phoenix_mcp::server
