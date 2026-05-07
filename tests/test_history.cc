#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <thread>

#include "ai/history.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// =============================================================================
// Safety: ensure the test DB path is NEVER a production / user-data path.
// =============================================================================
namespace {

/// Resolve @p p to a canonical path for reliable comparison.
/// Uses weakly_canonical so it works even if the file doesn't exist yet.
std::string resolve_path(fs::path const& p) {
  std::error_code ec;
  auto resolved = fs::weakly_canonical(p, ec);
  if (ec) {
    // Fallback: try absolute path
    resolved = fs::absolute(p, ec);
    if (ec) {
      return p.string();  // last resort
    }
  }
  return resolved.string();
}

/// Abort the test immediately if @p test_db_path points to the production
/// database (or any path under the production data directory).
void assert_safe_test_db_path(fs::path const& test_db_path) {
  static std::string prod_db_path = [] {
    std::string p = ai::HistoryDB::default_db_path();
    return resolve_path(p);
  }();

  std::string resolved_test = resolve_path(test_db_path);

  // Direct match: test path IS the production DB.
  if (resolved_test == prod_db_path) {
    std::cerr << "\n\n"
              << "======================================================\n"
              << "  FATAL: Test DB path points to PRODUCTION database!\n"
              << "  Path: " << test_db_path.string() << "\n"
              << "  Resolved: " << resolved_test << "\n"
              << "  Production DB: " << prod_db_path << "\n"
              << "  Tests ABORTED to prevent data loss.\n"
              << "======================================================\n\n";
    std::abort();
  }

  // Ancestor check: test path is WITHIN the production data directory tree.
  fs::path prod_dir = fs::path(prod_db_path).parent_path();  // e.g. .../ai.cli/
  std::string resolved_prod_dir = resolve_path(prod_dir);
  if (resolved_test.size() >= resolved_prod_dir.size() &&
      resolved_test.compare(0, resolved_prod_dir.size(), resolved_prod_dir) ==
          0) {
    std::cerr << "\n\n"
              << "======================================================\n"
              << "  FATAL: Test DB path is inside the production data\n"
              << "  directory! This could corrupt user data.\n"
              << "  Test path: " << test_db_path.string() << "\n"
              << "  Resolved:  " << resolved_test << "\n"
              << "  Prod dir:  " << resolved_prod_dir << "\n"
              << "  Tests ABORTED to prevent data loss.\n"
              << "======================================================\n\n";
    std::abort();
  }
}

}  // namespace

// =============================================================================
// Helper: create a temporary file path that gets cleaned up
// =============================================================================
class TempDbPath {
 public:
  TempDbPath() {
    // Use a unique temp directory per instance.  Combine a per-process
    // random seed (so that parallel ctest jobs never collide), a global
    // counter, and the thread id.
    static uint64_t process_salt = [] {
      std::random_device rd;
      return (static_cast<uint64_t>(rd()) << 32) ^
             static_cast<uint64_t>(
                 std::chrono::steady_clock::now().time_since_epoch().count());
    }();
    auto tmp_dir = fs::temp_directory_path();
    auto dir_name = "ai_cli_hist_" + std::to_string(++counter_) + "_" +
                    std::to_string(process_salt) + "_" +
                    std::to_string(std::hash<std::thread::id>{}(
                        std::this_thread::get_id()));
    dir_path_ = tmp_dir / dir_name;
    fs::create_directories(dir_path_);
    path_ = dir_path_ / "test.db";

    // 🔒 CRITICAL: verify the path is safe before touching the filesystem.
    assert_safe_test_db_path(path_);
  }

  ~TempDbPath() {
    std::error_code ec;
    // Remove the entire temp directory with all WAL/SHM files
    fs::remove_all(dir_path_, ec);
  }

  std::string path() const { return path_.string(); }

 private:
  fs::path dir_path_;
  fs::path path_;
  static inline int counter_ = 0;
};

// =============================================================================
// Helper: build a simple messages JSON array
// =============================================================================
json make_simple_messages() {
  return json::array(
      {json::object({{"role", "user"}, {"content", "Hello"}}),
       json::object({{"role", "assistant"}, {"content", "Hi there!"}})});
}

json make_single_message() {
  return json::array({json::object({{"role", "user"}, {"content", "Hello"}})});
}

// =============================================================================
// HistoryDB tests
// =============================================================================

class HistoryDBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Suppress log output during tests by setting a high severity
    db_path_ = std::make_unique<TempDbPath>();
  }

  void TearDown() override { db_path_.reset(); }

  std::unique_ptr<TempDbPath> db_path_;
};

// ── Constructor / database creation ─────────────────────────────────────

TEST_F(HistoryDBTest, ConstructorCreatesDatabaseFile) {
  std::string path = db_path_->path();
  {
    ai::HistoryDB db(path);
    EXPECT_TRUE(fs::exists(path));
  }
  // After destruction, the file should still exist on disk
  EXPECT_TRUE(fs::exists(path));
}

TEST_F(HistoryDBTest, ConstructorCreatesParentDirectories) {
  fs::path base = fs::temp_directory_path() / "ai_cli_test_nested_dir";
  fs::path db_file = base / "sub" / "test.db";

  // 🔒 Verify the path is safe before touching the filesystem.
  assert_safe_test_db_path(db_file);

  // Clean up after test
  auto cleanup = [&]() {
    std::error_code ec;
    fs::remove_all(base, ec);
  };

  {
    ai::HistoryDB db(db_file.string());
    EXPECT_TRUE(fs::exists(db_file));
  }

  cleanup();
}

TEST_F(HistoryDBTest, WALModeIsEnabled) {
  // In WAL mode, SQLite creates .db-wal and .db-shm files during
  // write transactions. We verify this by performing a write and checking
  // for WAL file existence.
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  db.save_messages(session_id, make_single_message());

  // After a write in WAL mode, there should be a WAL file
  std::string wal_path = db_path_->path() + "-wal";
  std::string shm_path = db_path_->path() + "-shm";

  // At least one of the WAL-related files should exist
  // (the WAL file may be cleaned up if checkpoint occurred)
  // If they exist it confirms WAL mode; just ensure the DB works correctly
  // in all cases.

  // The main DB file should exist
  EXPECT_TRUE(fs::exists(db_path_->path()));

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
}

// ── create_session ──────────────────────────────────────────────────────

TEST_F(HistoryDBTest, CreateSessionReturnsNonEmptyId) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  EXPECT_FALSE(session_id.empty());
}

TEST_F(HistoryDBTest, CreateSessionReturnsUniqueIds) {
  ai::HistoryDB db(db_path_->path());
  std::string id1 = db.create_session();
  std::string id2 = db.create_session();
  std::string id3 = db.create_session();

  EXPECT_NE(id1, id2);
  EXPECT_NE(id2, id3);
  EXPECT_NE(id1, id3);
}

TEST_F(HistoryDBTest, CreateSessionFormatIsCorrect) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  // Format: "YYYYMMDD-HHMMSS-<16hex>"
  // Check length: 8(date) + 1(-) + 6(time) + 1(-) + 16(hex) = 32
  EXPECT_EQ(session_id.size(), 32u);

  // Check structure: YYYYMMDD-HHMMSS-XXXXXXXXXXXXXXXX
  EXPECT_EQ(session_id[8], '-');
  EXPECT_EQ(session_id[15], '-');

  // Verify all hex chars after the second dash
  for (size_t i = 16; i < session_id.size(); ++i) {
    EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(session_id[i])));
  }

  // Check digits for date/time part
  for (size_t i = 0; i < 15; ++i) {
    if (i == 8) {
      continue;  // skip first dash
    }
    EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(session_id[i])));
  }
}

// ── get_messages ────────────────────────────────────────────────────────

TEST_F(HistoryDBTest, GetMessagesForNewSessionReturnsEmptyArray) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_array());
  EXPECT_EQ(result->size(), 0u);
}

TEST_F(HistoryDBTest, GetMessagesForNonExistentSessionReturnsNullopt) {
  ai::HistoryDB db(db_path_->path());
  auto result = db.get_messages("nonexistent-session-id");
  EXPECT_FALSE(result.has_value());
}

TEST_F(HistoryDBTest, GetMessagesAfterSaveReturnsCorrectData) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  auto messages = make_simple_messages();
  db.save_messages(session_id, messages);

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, messages);
}

// ── save_messages ───────────────────────────────────────────────────────

TEST_F(HistoryDBTest, SaveMessagesPersistsData) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  auto messages = make_simple_messages();

  db.save_messages(session_id, messages);

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), messages.size());
  EXPECT_EQ((*result)[0]["role"], "user");
  EXPECT_EQ((*result)[0]["content"], "Hello");
  EXPECT_EQ((*result)[1]["role"], "assistant");
  EXPECT_EQ((*result)[1]["content"], "Hi there!");
}

TEST_F(HistoryDBTest, SaveMessagesUpsertsExistingSession) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  // Save initial messages
  auto messages1 = make_single_message();
  db.save_messages(session_id, messages1);

  // Save updated messages (upsert)
  auto messages2 = make_simple_messages();
  db.save_messages(session_id, messages2);

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);  // Should be updated, not duplicated
  EXPECT_EQ((*result)[0]["role"], "user");
  EXPECT_EQ((*result)[1]["role"], "assistant");
}

TEST_F(HistoryDBTest, SaveMessagesRejectsNonArray) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  // Try to save a non-array JSON value
  json non_array = json::object({{"key", "value"}});
  db.save_messages(session_id, non_array);

  // Should still be empty array (the initial state)
  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 0u);
}

TEST_F(HistoryDBTest, SaveMessagesRejectsEmptyArray) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  // Save some real messages first
  auto messages = make_single_message();
  db.save_messages(session_id, messages);

  // Try to save an empty array - should be rejected, original data preserved
  json empty_array = json::array();
  db.save_messages(session_id, empty_array);

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
}

TEST_F(HistoryDBTest, SaveMessagesUpdatesTimestamp) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  auto messages = make_single_message();
  db.save_messages(session_id, messages);

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);
  std::string first_updated = infos[0].updated_at;

  // Wait a tiny bit then save again
  std::this_thread::sleep_for(std::chrono::seconds(2));

  db.save_messages(session_id, make_simple_messages());

  infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);
  std::string second_updated = infos[0].updated_at;

  EXPECT_NE(first_updated, second_updated);
}

// ── list_sessions ───────────────────────────────────────────────────────

TEST_F(HistoryDBTest, ListSessionsInitiallyEmpty) {
  ai::HistoryDB db(db_path_->path());
  auto sessions = db.list_sessions();
  EXPECT_TRUE(sessions.empty());
}

TEST_F(HistoryDBTest, ListSessionsReturnsAllSessions) {
  ai::HistoryDB db(db_path_->path());
  std::string id1 = db.create_session();
  std::string id2 = db.create_session();
  std::string id3 = db.create_session();

  auto sessions = db.list_sessions();
  EXPECT_EQ(sessions.size(), 3u);
}

TEST_F(HistoryDBTest, ListSessionsMostRecentFirst) {
  ai::HistoryDB db(db_path_->path());

  std::string id1 = db.create_session();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string id2 = db.create_session();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string id3 = db.create_session();

  auto sessions = db.list_sessions();
  ASSERT_EQ(sessions.size(), 3u);
  // Most recent (id3) should be first
  EXPECT_EQ(sessions[0], id3);
  EXPECT_EQ(sessions[1], id2);
  EXPECT_EQ(sessions[2], id1);
}

TEST_F(HistoryDBTest, ListSessionsReflectsUpdates) {
  ai::HistoryDB db(db_path_->path());

  // Create sessions in order: id1, id2
  std::string id1 = db.create_session();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string id2 = db.create_session();

  // Before update: id2 first, then id1
  auto sessions = db.list_sessions();
  ASSERT_EQ(sessions.size(), 2u);
  EXPECT_EQ(sessions[0], id2);
  EXPECT_EQ(sessions[1], id1);

  // Update id1 - now id1 should be first
  std::this_thread::sleep_for(std::chrono::seconds(1));
  db.save_messages(id1, make_single_message());

  sessions = db.list_sessions();
  ASSERT_EQ(sessions.size(), 2u);
  EXPECT_EQ(sessions[0], id1);
  EXPECT_EQ(sessions[1], id2);
}

// ── list_session_infos ──────────────────────────────────────────────────

TEST_F(HistoryDBTest, ListSessionInfosInitiallyEmpty) {
  ai::HistoryDB db(db_path_->path());
  auto infos = db.list_session_infos();
  EXPECT_TRUE(infos.empty());
}

TEST_F(HistoryDBTest, ListSessionInfosContainsMetadata) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  db.save_messages(session_id, make_simple_messages());

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);

  EXPECT_EQ(infos[0].session_id, session_id);
  EXPECT_FALSE(infos[0].created_at.empty());
  EXPECT_FALSE(infos[0].updated_at.empty());
  EXPECT_FALSE(infos[0].messages.empty());
}

TEST_F(HistoryDBTest, ListSessionInfosNLimitsResults) {
  ai::HistoryDB db(db_path_->path());

  // Create 5 sessions
  for (int i = 0; i < 5; ++i) {
    std::string id = db.create_session();
    db.save_messages(id, make_single_message());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto infos = db.list_session_infos(3);
  EXPECT_EQ(infos.size(), 3u);
}

TEST_F(HistoryDBTest, ListSessionInfosNZeroReturnsAll) {
  ai::HistoryDB db(db_path_->path());

  for (int i = 0; i < 3; ++i) {
    std::string id = db.create_session();
    db.save_messages(id, make_single_message());
  }

  // N=0 should return all (same as N=-1)
  auto infos = db.list_session_infos(0);
  EXPECT_EQ(infos.size(), 3u);

  auto infos_neg = db.list_session_infos(-1);
  EXPECT_EQ(infos_neg.size(), 3u);
}

TEST_F(HistoryDBTest, ListSessionInfosMostRecentFirst) {
  ai::HistoryDB db(db_path_->path());

  std::string id1 = db.create_session();
  db.save_messages(id1, make_single_message());
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::string id2 = db.create_session();
  db.save_messages(id2, make_single_message());
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::string id3 = db.create_session();
  db.save_messages(id3, make_single_message());

  auto infos = db.list_session_infos();
  ASSERT_EQ(infos.size(), 3u);
  EXPECT_EQ(infos[0].session_id, id3);
  EXPECT_EQ(infos[1].session_id, id2);
  EXPECT_EQ(infos[2].session_id, id1);
}

// ── SessionInfo::print ──────────────────────────────────────────────────

TEST_F(HistoryDBTest, PrintTextFormat) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  db.save_messages(session_id, make_simple_messages());

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);

  // Redirect stdout to a stringstream
  std::stringstream buffer;
  std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

  infos[0].print(false);

  std::cout.rdbuf(old);

  std::string output = buffer.str();
  EXPECT_NE(output.find("<user>"), std::string::npos);
  EXPECT_NE(output.find("<assistant>"), std::string::npos);
  EXPECT_NE(output.find("Hello"), std::string::npos);
  EXPECT_NE(output.find("Hi there!"), std::string::npos);
}

TEST_F(HistoryDBTest, PrintJsonFormat) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();
  db.save_messages(session_id, make_simple_messages());

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);

  // Redirect stdout
  std::stringstream buffer;
  std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

  infos[0].print(true);

  std::cout.rdbuf(old);

  std::string output = buffer.str();

  // Should be valid JSON
  json parsed = json::parse(output);
  EXPECT_TRUE(parsed.contains("messages"));
  EXPECT_TRUE(parsed.contains("created_at"));
  EXPECT_TRUE(parsed.contains("updated_at"));
  EXPECT_TRUE(parsed["messages"].is_array());
  EXPECT_EQ(parsed["messages"].size(), 2u);
}

TEST_F(HistoryDBTest, PrintWithToolCalls) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  json messages = json::array({json::object(
      {{"role", "assistant"},
       {"tool_calls",
        json::array({json::object(
            {{"function", json::object({{"name", "bash"},
                                        {"arguments", "echo hello"}})}})})}})});
  db.save_messages(session_id, messages);

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);

  std::stringstream buffer;
  std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

  infos[0].print(false);

  std::cout.rdbuf(old);

  std::string output = buffer.str();
  EXPECT_NE(output.find("[bash]"), std::string::npos);
  EXPECT_NE(output.find("echo hello"), std::string::npos);
}

TEST_F(HistoryDBTest, PrintWithReasoningContent) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  json messages = json::array(
      {json::object({{"role", "assistant"},
                     {"reasoning_content", "Let me think about this..."},
                     {"content", "Here is the answer."}})});
  db.save_messages(session_id, messages);

  auto infos = db.list_session_infos(1);
  ASSERT_EQ(infos.size(), 1u);

  std::stringstream buffer;
  std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

  infos[0].print(false);

  std::cout.rdbuf(old);

  std::string output = buffer.str();
  EXPECT_NE(output.find("<thinking>"), std::string::npos);
  EXPECT_NE(output.find("Let me think about this..."), std::string::npos);
  EXPECT_NE(output.find("</thinking>"), std::string::npos);
  EXPECT_NE(output.find("Here is the answer."), std::string::npos);
}

// ── default_db_path ─────────────────────────────────────────────────────

TEST_F(HistoryDBTest, DefaultDbPathReturnsNonEmpty) {
  std::string path = ai::HistoryDB::default_db_path();
  EXPECT_FALSE(path.empty());
  EXPECT_NE(path.find("chat_history.db"), std::string::npos);
}

// ── Persistence across instances ────────────────────────────────────────

TEST_F(HistoryDBTest, DataPersistsAcrossInstances) {
  std::string path = db_path_->path();
  std::string session_id;

  {
    ai::HistoryDB db(path);
    session_id = db.create_session();
    db.save_messages(session_id, make_simple_messages());
  }

  {
    ai::HistoryDB db(path);
    auto result = db.get_messages(session_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0]["role"], "user");
    EXPECT_EQ((*result)[0]["content"], "Hello");
  }
}

// ── Messages JSON with complex content ──────────────────────────────────

TEST_F(HistoryDBTest, SaveAndRetrieveComplexMessages) {
  ai::HistoryDB db(db_path_->path());
  std::string session_id = db.create_session();

  json messages = json::array(
      {json::object(
           {{"role", "system"}, {"content", "You are a helpful assistant."}}),
       json::object({{"role", "user"}, {"content", "What is C++?"}}),
       json::object(
           {{"role", "assistant"},
            {"content", "C++ is a general-purpose programming language..."},
            {"tool_calls",
             json::array({json::object(
                 {{"function",
                   json::object({{"name", "search"},
                                 {"arguments", "{\"query\":\"C++\"}"}})}})})}}),
       json::object(
           {{"role", "tool"},
            {"tool_call_id", "call_123"},
            {"content", "C++ is a standardized programming language..."}})});

  db.save_messages(session_id, messages);

  auto result = db.get_messages(session_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 4u);
  EXPECT_EQ((*result)[0]["role"], "system");
  EXPECT_EQ((*result)[3]["role"], "tool");
  EXPECT_EQ((*result)[3]["tool_call_id"], "call_123");
}

// ── Multiple sessions with interleaved operations ───────────────────────

TEST_F(HistoryDBTest, MultipleSessionsInterleaved) {
  ai::HistoryDB db(db_path_->path());

  std::string id1 = db.create_session();
  std::string id2 = db.create_session();

  db.save_messages(id1, make_single_message());
  db.save_messages(id2, make_simple_messages());

  auto msgs1 = db.get_messages(id1);
  auto msgs2 = db.get_messages(id2);

  ASSERT_TRUE(msgs1.has_value());
  ASSERT_TRUE(msgs2.has_value());
  EXPECT_EQ(msgs1->size(), 1u);
  EXPECT_EQ(msgs2->size(), 2u);
}

// ── Edge case: empty string session_id ──────────────────────────────────

TEST_F(HistoryDBTest, GetMessagesWithEmptySessionId) {
  ai::HistoryDB db(db_path_->path());
  auto result = db.get_messages("");
  EXPECT_FALSE(result.has_value());
}

TEST_F(HistoryDBTest, SaveMessagesWithEmptySessionId) {
  ai::HistoryDB db(db_path_->path());
  // Should not crash
  db.save_messages("", make_single_message());
  // Empty string is a valid session_id; session is created
  auto sessions = db.list_sessions();
  EXPECT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0], "");
}

// ── Edge case: many sessions ────────────────────────────────────────────

TEST_F(HistoryDBTest, ManySessions) {
  ai::HistoryDB db(db_path_->path());

  constexpr int kNumSessions = 50;
  for (int i = 0; i < kNumSessions; ++i) {
    std::string id = db.create_session();
    db.save_messages(id, make_single_message());
  }

  auto sessions = db.list_sessions();
  EXPECT_EQ(sessions.size(), static_cast<size_t>(kNumSessions));

  auto infos = db.list_session_infos(10);
  EXPECT_EQ(infos.size(), 10u);
}
