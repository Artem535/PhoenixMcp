#include "phoenix_mcp/server/task_store.h"

#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <thread>

using namespace phoenix_mcp::server;
using namespace std::chrono_literals;

namespace {

rfl::Generic text_result(const std::string& text) { return rfl::Generic{text}; }

}  // namespace

TEST(TaskStoreTest, CreateStartsWorking) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());
  EXPECT_FALSE(created->task_id.empty());
  EXPECT_EQ(created->state, TaskState::Working);
  EXPECT_STREQ(to_string(created->state), "working");
  EXPECT_FALSE(created->created_at.empty());
  EXPECT_EQ(created->created_at, created->updated_at);
}

TEST(TaskStoreTest, CreateRespectsRequestedTtl) {
  TaskStore store;
  const auto created = store.create(5000);
  ASSERT_TRUE(created.has_value());
  ASSERT_TRUE(created->ttl_ms.has_value());
  EXPECT_EQ(*created->ttl_ms, 5000);
}

TEST(TaskStoreTest, CompleteStoresResult) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());

  store.complete(created->task_id, text_result("ok"));

  const auto snapshot = store.get(created->task_id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::Completed);
  ASSERT_TRUE(snapshot->result.has_value());
  EXPECT_FALSE(snapshot->updated_at.empty());
}

TEST(TaskStoreTest, FailRetainsResult) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());

  store.fail(created->task_id, "Tool returned an error", text_result("bad"));

  const auto snapshot = store.get(created->task_id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::Failed);
  EXPECT_EQ(snapshot->status_message, "Tool returned an error");
  ASSERT_TRUE(snapshot->result.has_value());
}

TEST(TaskStoreTest, CancelWorkingTask) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());

  TaskSnapshot out;
  EXPECT_EQ(store.cancel(created->task_id, "bye", &out),
            CancelStatus::Cancelled);
  EXPECT_EQ(out.state, TaskState::Cancelled);
  EXPECT_EQ(out.status_message, "bye");

  // A late completion must not overwrite the terminal cancelled state.
  store.complete(created->task_id, text_result("late"));
  const auto snapshot = store.get(created->task_id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::Cancelled);
}

TEST(TaskStoreTest, CancelTerminalTaskReportsAlreadyTerminal) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());
  store.complete(created->task_id, text_result("done"));

  TaskSnapshot out;
  EXPECT_EQ(store.cancel(created->task_id, "too late", &out),
            CancelStatus::AlreadyTerminal);
  EXPECT_EQ(out.state, TaskState::Completed);
}

TEST(TaskStoreTest, CancelUnknownTaskReportsNotFound) {
  TaskStore store;
  TaskSnapshot out;
  EXPECT_EQ(store.cancel("missing", "bye", &out), CancelStatus::NotFound);
}

TEST(TaskStoreTest, ListReturnsTasksByCreationTime) {
  TaskStore store;
  const auto first = store.create(std::nullopt);
  const auto second = store.create(std::nullopt);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  const auto tasks = store.list();
  ASSERT_EQ(tasks.size(), 2u);
  EXPECT_EQ(tasks[0].task_id, first->task_id);
  EXPECT_EQ(tasks[1].task_id, second->task_id);
}

TEST(TaskStoreTest, TtlExpiryRemovesTask) {
  TaskStore::Options options;
  options.default_ttl = 10ms;
  options.max_ttl = 10ms;
  TaskStore store(options);

  const auto created = store.create(10);
  ASSERT_TRUE(created.has_value());
  std::this_thread::sleep_for(30ms);

  EXPECT_FALSE(store.get(created->task_id).has_value());
  EXPECT_EQ(store.size(), 0u);
}

TEST(TaskStoreTest, CapacityRejectsWhenNoTerminalTaskToEvict) {
  TaskStore::Options options;
  options.max_tasks = 1;
  TaskStore store(options);

  const auto first = store.create(std::nullopt);
  ASSERT_TRUE(first.has_value());
  EXPECT_FALSE(store.create(std::nullopt).has_value());

  store.complete(first->task_id, text_result("done"));
  const auto second = store.create(std::nullopt);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(store.size(), 1u);
  EXPECT_FALSE(store.get(first->task_id).has_value());
}

TEST(TaskStoreTest, WaitForTerminalBlocksUntilCompletion) {
  TaskStore store;
  const auto created = store.create(std::nullopt);
  ASSERT_TRUE(created.has_value());

  std::thread worker([&store, task_id = created->task_id] {
    std::this_thread::sleep_for(20ms);
    store.complete(task_id, text_result("ok"));
  });

  const auto snapshot = store.wait_for_terminal(created->task_id, 1000ms);
  worker.join();

  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::Completed);
}

TEST(TaskStoreTest, GeneratesUniqueIds) {
  TaskStore store;
  std::set<std::string> ids;
  for (int index = 0; index < 16; ++index) {
    const auto created = store.create(std::nullopt);
    ASSERT_TRUE(created.has_value());
    ids.insert(created->task_id);
  }
  EXPECT_EQ(ids.size(), 16u);
}

TEST(TaskStoreTest, CancelAllCancelsLiveTasks) {
  TaskStore store;
  const auto first = store.create(std::nullopt);
  const auto second = store.create(std::nullopt);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  store.complete(first->task_id, text_result("done"));

  store.cancel_all();

  EXPECT_EQ(store.get(second->task_id)->state, TaskState::Cancelled);
  EXPECT_EQ(store.get(first->task_id)->state, TaskState::Completed);
}
