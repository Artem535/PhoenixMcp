#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <rfl/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

#include "phoenix_mcp/server/server_session.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"

using namespace phoenix_mcp::server;
using namespace phoenix_mcp::msg::types;

namespace {

constexpr auto kInitializeRequest =
    R"({"jsonrpc":"2.0","method":"initialize","id":1,)"
    R"("params":{"protocolVersion":"2025-11-25","capabilities":{},)"
    R"("clientInfo":{"name":"test-client","version":"0.0.0"}}})";
constexpr auto kInitializedNotification =
    R"({"jsonrpc":"2.0","method":"notifications/initialized"})";

std::unique_ptr<ServerSession> make_session(
    std::unique_ptr<phoenix_mcp::tool::ToolRegistry> registry = nullptr) {
  ServerCapabilities capabilities{.tools =
                                      ToolsCapabilities{.list_changed = false}};
  Implementation info{.name = "test", .version = "0.0.0"};
  if (!registry) {
    registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  }
  return std::make_unique<ServerSession>(capabilities, info, "instruction",
                                         std::move(registry));
}

void initialize(ServerSession& session) {
  session.handle_input(kInitializeRequest);
  session.handle_input(kInitializedNotification);
}

struct SumInput {
  int a;
  int b;
};

struct OptionalInput {
  std::optional<int> value;
};

struct ThrowingInput {
  int unused;
};

}  // namespace

TEST(ServerSessionToolErrorsTest, InvalidArgumentsReturnToolError) {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<SumInput>(
      "sum", "Sum two ints", [](const SumInput& input) {
        return phoenix_mcp::utils::make_text_result(
            std::to_string(input.a + input.b));
      });
  const auto session = make_session(std::move(registry));
  initialize(*session);

  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"tools/call","id":10,)"
                            R"("params":{"name":"sum","arguments":{"a":1}}})");
  ASSERT_TRUE(response.has_value());

  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("\"isError\":true"), std::string::npos);
  EXPECT_EQ(json.find("\"error\":"), std::string::npos);
  EXPECT_NE(json.find("Invalid arguments"), std::string::npos);
}

TEST(ServerSessionToolErrorsTest, UnknownToolReturnsProtocolError) {
  const auto session = make_session();
  initialize(*session);

  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"tools/call","id":11,)"
                            R"("params":{"name":"missing"}})");
  ASSERT_TRUE(response.has_value());

  const auto error = rfl::json::read<Error>(rfl::json::write(*response));
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->error.code, -32602);
}

TEST(ServerSessionToolErrorsTest, ToolExceptionReturnsToolError) {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_async_tool<ThrowingInput>(
      "boom", "Always throws",
      [](const ThrowingInput&) -> folly::coro::Task<CallToolResult> {
        throw std::runtime_error("boom");
        co_return CallToolResult{
            .content = {TextContent{.text = "unreachable"}}};
      });
  const auto session = make_session(std::move(registry));
  initialize(*session);

  const auto response = session->handle_input(
      R"({"jsonrpc":"2.0","method":"tools/call","id":12,)"
      R"("params":{"name":"boom","arguments":{"unused":0}}})");
  ASSERT_TRUE(response.has_value());

  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("\"isError\":true"), std::string::npos);
  EXPECT_EQ(json.find("\"error\":"), std::string::npos);
  EXPECT_NE(json.find("boom"), std::string::npos);
}

TEST(ServerSessionToolErrorsTest, MissingArgumentsTreatedAsEmptyObject) {
  auto registry = std::make_unique<phoenix_mcp::tool::ToolRegistry>();
  registry->register_tool<OptionalInput>(
      "optional_tool", "Accepts no arguments", [](const OptionalInput& input) {
        return phoenix_mcp::utils::make_text_result(
            input.value.has_value() ? std::to_string(*input.value) : "none");
      });
  const auto session = make_session(std::move(registry));
  initialize(*session);

  const auto response =
      session->handle_input(R"({"jsonrpc":"2.0","method":"tools/call","id":13,)"
                            R"("params":{"name":"optional_tool"}})");
  ASSERT_TRUE(response.has_value());

  const auto json = rfl::json::write(*response);
  EXPECT_NE(json.find("none"), std::string::npos);
  EXPECT_EQ(json.find("\"error\":"), std::string::npos);
}
