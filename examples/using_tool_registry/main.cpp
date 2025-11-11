#include <rfl/Generic.hpp>
#include <rfl/json.hpp>

#include "../../src/phoenix_mcp/tool_registry/utils.hpp"
#include "../../src/phoenix_mcp/types/msg_types.hpp"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "spdlog/spdlog.h"

struct Test {
  int a;
  int b;
};

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  // Create tool registry.
  pxm::tool::ToolRegistry registry;
  // Register function.
  registry.registerTool<Test>("test", "", [](const Test& test) {
    rfl::Generic::Object obj;
    obj["a"] = test.a + 1;
    obj["b"] = test.b + 2;
    return pxm::utils::make_text_result(rfl::json::write(obj));
  });

  // Create object with params to function.
  Test test{0, 0};
  // Call function with convertion to Generic.
  const auto result = registry.call_tool("test", rfl::to_generic(test));
  // Convert result(Generic) to object.
  // Print result.
  spdlog::info("Result: {}", rfl::json::write(result));
  return 0;
}