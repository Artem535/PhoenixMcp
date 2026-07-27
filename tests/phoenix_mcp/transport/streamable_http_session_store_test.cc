#include "streamable_http_session_store.h"

#include <gtest/gtest.h>

namespace phoenix_mcp::server::streamable_http_internal {
namespace {

TEST(StreamableHttpSessionStoreTest, CreatesDistinctVisibleAsciiSessionIds) {
  StreamableHttpSessionStore store;

  const auto first = store.create_session();
  const auto second = store.create_session();

  EXPECT_NE(first, second);
  EXPECT_TRUE(store.contains_session(first));
  EXPECT_TRUE(store.contains_session(second));
}

TEST(StreamableHttpSessionStoreTest, ReplaysOnlyEventsAfterCursor) {
  StreamableHttpSessionStore store;
  const auto session = store.create_session();
  const auto stream = store.create_stream(session);
  ASSERT_TRUE(stream.has_value());

  const auto first = store.append(session, *stream, R"({"method":"first"})");
  const auto second = store.append(session, *stream, R"({"method":"second"})");
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  const auto replay = store.replay(session, first->id);
  ASSERT_EQ(replay.status, ReplayStatus::kOk);
  ASSERT_EQ(replay.events.size(), 1U);
  EXPECT_EQ(replay.events.front().id, second->id);
}

TEST(StreamableHttpSessionStoreTest, EvictedCursorRequiresResync) {
  StreamableHttpSessionStore store;
  const auto session = store.create_session();
  const auto stream = store.create_stream(session);
  ASSERT_TRUE(stream.has_value());

  std::optional<StoredEvent> first;
  for (int index = 0; index < 1025; ++index) {
    const auto event = store.append(session, *stream, "{}");
    ASSERT_TRUE(event.has_value());
    if (index == 0) {
      first = event;
    }
  }

  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(store.replay(session, first->id).status,
            ReplayStatus::kResyncRequired);
}

}  // namespace
}  // namespace phoenix_mcp::server::streamable_http_internal
