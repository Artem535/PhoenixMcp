//
// Created by artem.d on 11.11.2025.
//
#pragma once
#include "../types/msg_types.hpp"


namespace pxm::utils {
inline msg::types::CallToolResult make_text_result(std::string text,
                                                 bool is_error = false) {
  msg::types::TextContent txt{.text = std::move(text)};
  std::vector<msg::types::VariantContent> cv{std::move(txt)};
  msg::types::CallToolResult result{
      .content = cv,
      .is_error = is_error
  };
  return result;
}

// то же для картинки, если понадобится
inline msg::types::CallToolResult make_image_result(std::string base64,
                                                  std::string mime,
                                                  bool is_error = false) {
  msg::types::ImageContent img{.data = std::move(base64),
                               .mime_type = std::move(mime)};
  std::vector<msg::types::VariantContent> cv{std::move(img)};
  msg::types::CallToolResult result{
      .content = cv,
      .is_error = is_error
  };

  return result;
}
}