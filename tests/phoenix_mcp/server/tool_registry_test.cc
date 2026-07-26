#include "phoenix_mcp/tool_registry/tool_registry.h"

#include <gtest/gtest.h>

#include <rfl/Generic.hpp>
#include <string>

namespace phoenix_mcp::tool {
namespace {

struct AddInput {
  int left;
  int right;
};

TEST(ToolRegistryTest, RegistersListsAndCallsTypedTool) {
  ToolRegistry registry;
  registry.register_tool<AddInput>(
      "add", "Adds two values", [](const AddInput& input) {
        return utils::make_text_result(
            std::to_string(input.left + input.right));
      });

  const auto tools = registry.get_tool_list();
  ASSERT_EQ(tools.size(), 1U);
  EXPECT_EQ(tools.front().name, "add");

  AddInput input{.left = 2, .right = 3};
  const auto result = registry.call_tool("add", rfl::to_generic(input));
  ASSERT_FALSE(result.content.empty());
  EXPECT_FALSE(result.is_error.get().value_or(false));
}

}  // namespace
}  // namespace phoenix_mcp::tool
