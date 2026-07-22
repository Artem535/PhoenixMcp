#ifndef PHOENIX_MCP_TOOL_REGISTRY_UTILS_H_
#define PHOENIX_MCP_TOOL_REGISTRY_UTILS_H_

#include <spdlog/spdlog.h>

#include <string>
#include <utility>
#include <vector>

#include "phoenix_mcp/protocol/message_types.h"

namespace phoenix_mcp::utils {

inline msg::types::CallToolResult make_text_result(std::string text,
                                                   bool is_error = false) {
  spdlog::debug("make_text_result| input text {}", text);
  msg::types::TextContent content{.text = std::move(text)};
  return {.content = {std::move(content)}, .is_error = is_error};
}

inline msg::types::CallToolResult make_image_result(std::string base64,
                                                    std::string mime,
                                                    bool is_error = false) {
  msg::types::ImageContent content{.data = std::move(base64),
                                   .mime_type = std::move(mime)};
  return {.content = {std::move(content)}, .is_error = is_error};
}

}  // namespace phoenix_mcp::utils

#endif  // PHOENIX_MCP_TOOL_REGISTRY_UTILS_H_
