#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "spdlog/spdlog.h"

struct Test {
  int a;
  int b;
};

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);

  pxm::tool::ToolRegistry registry;
  registry.registerTool<Test>("test", "", [](const Test &test){return "";});

  return 0;
}