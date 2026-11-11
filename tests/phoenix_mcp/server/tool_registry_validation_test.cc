#include <gtest/gtest.h>

#include <rfl/Generic.hpp>
#include <string>

#include "phoenix_mcp/tool_registry/tool_registry.h"

namespace phoenix_mcp::tool {
namespace {

struct AddInput {
  int a;
  int b;
};

}  // namespace

TEST(ToolRegistryTest, HasToolReflectsRegistration) {
  ToolRegistry registry;
  EXPECT_FALSE(registry.has_tool("sum"));

  registry.register_tool<AddInput>(
      "sum", "Sum two ints", [](const AddInput& input) {
        return phoenix_mcp::utils::make_text_result(
            std::to_string(input.a + input.b));
      });

  EXPECT_TRUE(registry.has_tool("sum"));
  EXPECT_FALSE(registry.has_tool("missing"));
}

TEST(ToolRegistryTest, InvalidArgumentsReturnIsErrorResult) {
  ToolRegistry registry;
  registry.register_tool<AddInput>(
      "sum", "Sum two ints", [](const AddInput& input) {
        return phoenix_mcp::utils::make_text_result(
            std::to_string(input.a + input.b));
      });

  // `b` is a required field and is missing, so schema validation must fail.
  rfl::Generic::Object params;
  params["a"] = rfl::Generic{1};
  const auto result =
      registry.call_tool("sum", rfl::Generic{std::move(params)});

  EXPECT_TRUE(result.is_error.value().value_or(false));
  const auto json = rfl::json::write(result);
  EXPECT_NE(json.find("Invalid arguments"), std::string::npos);
}

}  // namespace phoenix_mcp::tool
