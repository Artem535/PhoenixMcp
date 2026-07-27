#include "streamable_http_session_store.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

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

TEST(StreamableHttpSessionStoreTest, PublishesAndRetainsMessageForLiveStream) {
  StreamableHttpSessionStore store;
  const auto session = store.create_session();
  std::vector<StoredEvent> delivered;

  const auto stream =
      store.open_stream(session, [&delivered](const StoredEvent& event) {
        delivered.push_back(event);
        return true;
      });
  ASSERT_TRUE(stream.has_value());

  EXPECT_TRUE(store.publish(session, R"({"method":"notifications/progress"})"));
  ASSERT_EQ(delivered.size(), 1U);
  EXPECT_EQ(delivered.front().payload,
            R"({"method":"notifications/progress"})");

  const auto replay = store.replay(session, delivered.front().id);
  EXPECT_EQ(replay.status, ReplayStatus::kOk);
  EXPECT_TRUE(replay.events.empty());
}

TEST(StreamableHttpSessionStoreTest, PublishesToExactlyOneOfTwoLiveStreams) {
  StreamableHttpSessionStore store;
  const auto session = store.create_session();
  std::vector<StoredEvent> first_delivery;
  std::vector<StoredEvent> second_delivery;

  ASSERT_TRUE(
      store.open_stream(session, [&first_delivery](const StoredEvent& event) {
        first_delivery.push_back(event);
        return true;
      }));
  ASSERT_TRUE(
      store.open_stream(session, [&second_delivery](const StoredEvent& event) {
        second_delivery.push_back(event);
        return true;
      }));

  ASSERT_TRUE(store.publish(session, R"({"method":"notifications/log"})"));
  EXPECT_EQ(first_delivery.size() + second_delivery.size(), 1U);
}

TEST(StreamableHttpSessionStoreTest, ResumesStreamAndReplaysEventsAfterCursor) {
  StreamableHttpSessionStore store;
  const auto session = store.create_session();
  std::vector<StoredEvent> first_delivery;
  const auto stream =
      store.open_stream(session, [&first_delivery](const StoredEvent& event) {
        first_delivery.push_back(event);
        return true;
      });
  ASSERT_TRUE(stream.has_value());
  ASSERT_TRUE(store.publish(session, R"({"method":"first"})"));
  ASSERT_TRUE(store.publish(session, R"({"method":"second"})"));

  std::vector<StoredEvent> resumed_delivery;
  const auto replay =
      store.resume_stream(session, first_delivery.front().id,
                          [&resumed_delivery](const StoredEvent& event) {
                            resumed_delivery.push_back(event);
                            return true;
                          });

  ASSERT_TRUE(replay.has_value());
  EXPECT_EQ(replay->status, ReplayStatus::kOk);
  ASSERT_EQ(replay->events.size(), 1U);
  EXPECT_EQ(replay->events.front().id, first_delivery.back().id);
  ASSERT_TRUE(store.publish(session, R"({"method":"third"})"));
  ASSERT_EQ(resumed_delivery.size(), 1U);
  EXPECT_EQ(resumed_delivery.front().payload, R"({"method":"third"})");
}

}  // namespace
}  // namespace phoenix_mcp::server::streamable_http_internal
