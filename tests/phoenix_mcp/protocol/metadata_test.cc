#include <gtest/gtest.h>

#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "phoenix_mcp/protocol/message_types.h"

using namespace phoenix_mcp::msg::types;

TEST(MetadataTest, ToolSerializesIconsWhenSet) {
  const Tool tool{.name = "getUser",
                  .description = "Get a user",
                  .input_schema = InputSchema{},
                  .icons = std::vector<Icon>{Icon{
                      .src = "https://example.com/icon.png",
                      .mime_type = std::string("image/png"),
                      .sizes = std::vector<std::string>{"48x48", "96x96"}}}};

  const auto json = rfl::json::write(tool);
  EXPECT_NE(json.find("\"icons\":["), std::string::npos);
  EXPECT_NE(json.find("\"src\":\"https://example.com/icon.png\""),
            std::string::npos);
  EXPECT_NE(json.find("\"mimeType\":\"image/png\""), std::string::npos);
  EXPECT_NE(json.find("\"48x48\""), std::string::npos);
}

TEST(MetadataTest, ToolOmitsOptionalMetadataWhenUnset) {
  const Tool tool{.name = "getUser",
                  .description = std::nullopt,
                  .input_schema = InputSchema{}};

  const auto json = rfl::json::write(tool);
  EXPECT_EQ(json.find("\"icons\""), std::string::npos);
  EXPECT_EQ(json.find("\"description\""), std::string::npos);
}

TEST(MetadataTest, ImplementationSerializesDescriptionWhenSet) {
  const Implementation impl{
      .name = "server", .version = "1.0.0", .description = "A sample server"};

  const auto json = rfl::json::write(impl);
  EXPECT_NE(json.find("\"description\":\"A sample server\""),
            std::string::npos);
}

TEST(MetadataTest, ImplementationOmitsDescriptionWhenUnset) {
  const Implementation impl{.name = "server", .version = "1.0.0"};

  const auto json = rfl::json::write(impl);
  EXPECT_EQ(json.find("\"description\""), std::string::npos);
}

TEST(MetadataTest, IconRoundTrips) {
  const Icon icon{.src = "data:image/png;base64,abc",
                  .mime_type = std::string("image/png"),
                  .sizes = std::vector<std::string>{"24x24"}};

  const auto parsed = rfl::json::read<Icon>(rfl::json::write(icon));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->src, icon.src);
  ASSERT_TRUE(parsed->mime_type.value().has_value());
  EXPECT_EQ(parsed->mime_type.value().value(), "image/png");
  ASSERT_TRUE(parsed->sizes.has_value());
  EXPECT_EQ(parsed->sizes->at(0), "24x24");
}
