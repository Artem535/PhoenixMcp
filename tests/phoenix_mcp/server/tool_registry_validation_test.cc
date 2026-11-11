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

TEST(ToolRegistryTest, ValidatesToolNames) {
  EXPECT_TRUE(ToolRegistry::is_valid_tool_name("getUser"));
  EXPECT_TRUE(ToolRegistry::is_valid_tool_name("DATA_EXPORT_v2"));
  EXPECT_TRUE(ToolRegistry::is_valid_tool_name("admin.tools.list"));
  EXPECT_TRUE(ToolRegistry::is_valid_tool_name(std::string(128, 'a')));

  EXPECT_FALSE(ToolRegistry::is_valid_tool_name(""));
  EXPECT_FALSE(ToolRegistry::is_valid_tool_name(std::string(129, 'a')));
  EXPECT_FALSE(ToolRegistry::is_valid_tool_name("has space"));
  EXPECT_FALSE(ToolRegistry::is_valid_tool_name("has,comma"));
  EXPECT_FALSE(ToolRegistry::is_valid_tool_name("slash/name"));
  EXPECT_FALSE(ToolRegistry::is_valid_tool_name("at@name"));
}

TEST(ToolRegistryTest, RejectsInvalidToolNameAtRegistration) {
  ToolRegistry registry;
  EXPECT_THROW(registry.register_tool<AddInput>(
                   "bad name", "desc",
                   [](const AddInput& input) {
                     return phoenix_mcp::utils::make_text_result(
                         std::to_string(input.a + input.b));
                   }),
               std::invalid_argument);
  EXPECT_FALSE(registry.has_tool("bad name"));
}

TEST(ToolRegistryTest, SetToolIconsUpdatesDefinition) {
  ToolRegistry registry;
  registry.register_tool<AddInput>("sum", "Sum", [](const AddInput& input) {
    return phoenix_mcp::utils::make_text_result(
        std::to_string(input.a + input.b));
  });

  EXPECT_TRUE(registry.set_tool_icons(
      "sum", {msg::types::Icon{.src = "https://example.com/sum.png"}}));

  const auto tools = registry.get_tool_list();
  ASSERT_EQ(tools.size(), 1u);
  ASSERT_TRUE(tools.front().icons.has_value());
  EXPECT_EQ(tools.front().icons->front().src, "https://example.com/sum.png");

  EXPECT_FALSE(registry.set_tool_icons("missing", {}));
}

}  // namespace phoenix_mcp::tool
