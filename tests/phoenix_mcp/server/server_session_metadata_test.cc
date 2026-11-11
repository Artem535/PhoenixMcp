#include <gtest/gtest.h>

#include <memory>
#include <rfl/json.hpp>
#include <string>
#include <utility>

#include "phoenix_mcp/server/server_session.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

constexpr auto kInit20251125 =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-11-25","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
constexpr auto kInit20250618 =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-06-18","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
constexpr auto kInitialized =
    R"({"jsonrpc":"2.0","method":"notifications/initialized"})";

struct EchoInput {
  std::string text;
};

std::unique_ptr<ServerSession> make_session(
    Implementation info,
    std::unique_ptr<phoenix_mcp::tool::ToolRegistry> registry = nullptr) {
  ServerCapabilities capabilities{.tools =
                                      ToolsCapabilities{.list_changed = false}};
  if (!registry) {
    registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  }
  return std::make_unique<ServerSession>(capabilities, std::move(info),
                                         "instruction", std::move(registry));
}

std::unique_ptr<phoenix_mcp::tool::ToolRegistry> make_echo_registry() {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<EchoInput>(
      "echo", "Echo text", [](const EchoInput& input) {
        return phoenix_mcp::utils::make_text_result(input.text);
      });
  return registry;
}

}  // namespace

TEST(ServerSessionMetadataTest, InitializeIncludesImplementationDescription) {
  const auto session =
      make_session(Implementation{.name = "sample-server",
                                  .version = "1.2.3",
                                  .description = "A sample MCP server"});
  const auto response = session->handle_input(kInit20251125);
  ASSERT_TRUE(response.has_value());

  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("\"description\":\"A sample MCP server\""),
            std::string::npos);
  EXPECT_NE(json.find("\"name\":\"sample-server\""), std::string::npos);
}

TEST(ServerSessionMetadataTest, InitializeOmitsDescriptionWhenUnset) {
  const auto session =
      make_session(Implementation{.name = "sample-server", .version = "1.2.3"});
  const auto response = session->handle_input(kInit20251125);
  ASSERT_TRUE(response.has_value());

  EXPECT_EQ(rfl::json::write(*response).find("\"description\""),
            std::string::npos);
}

TEST(ServerSessionMetadataTest, ToolsListIncludesIconsWhenSet) {
  auto registry = make_echo_registry();
  ASSERT_TRUE(registry->set_tool_icons(
      "echo", {Icon{.src = "https://example.com/echo.png"}}));
  const auto session =
      make_session(Implementation{.name = "sample-server", .version = "1.0.0"},
                   std::move(registry));
  session->handle_input(kInit20251125);
  session->handle_input(kInitialized);

  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"tools/list","id":2})");
  ASSERT_TRUE(response.has_value());

  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("\"icons\":["), std::string::npos);
  EXPECT_NE(json.find("https://example.com/echo.png"), std::string::npos);
}

TEST(ServerSessionMetadataTest, ToolsListOmitsIconsWhenUnset) {
  const auto session =
      make_session(Implementation{.name = "sample-server", .version = "1.0.0"},
                   make_echo_registry());
  session->handle_input(kInit20251125);
  session->handle_input(kInitialized);

  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"tools/list","id":2})");
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(rfl::json::write(*response).find("\"icons\""), std::string::npos);
}

TEST(ServerSessionMetadataTest, Dialect20250618StillServesTools) {
  const auto session =
      make_session(Implementation{.name = "sample-server", .version = "1.0.0"},
                   make_echo_registry());
  session->handle_input(kInit20250618);
  session->handle_input(kInitialized);

  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"tools/list","id":2})");
  ASSERT_TRUE(response.has_value());
  EXPECT_NE(rfl::json::write(*response).find("\"name\":\"echo\""),
            std::string::npos);
}
