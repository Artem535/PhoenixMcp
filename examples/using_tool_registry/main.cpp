#include <rfl/Generic.hpp>

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
  registry.registerTool<Test>("test", "", [](const Test& test) {
    rfl::Generic::Object obj;
    obj["a"] = test.a + 1;
    obj["b"] = test.b + 2;
    return obj;
  });

  // Create object with params to function.
  Test test{0, 0};
  // Call function with convertion to Generic.
  const auto result = registry.callTool("test", rfl::to_generic(test));
  // Convert result(Generic) to object.
  const auto new_result = rfl::from_generic<Test>(result).value();
  // Print result.
  spdlog::info("Result: {} {}", new_result.a, new_result.b);
  return 0;
}