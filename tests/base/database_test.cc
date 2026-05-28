#include "base/database.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <filesystem>

#include "base/temp_dir.h"
#include "base/temp_file.h"

namespace fs = std::filesystem;

// =============================================================================
// database
// =============================================================================

TEST(DatabaseTest, ConstructorCreatesValidDatabase) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();

  ai::base::database db(db_path);
  EXPECT_TRUE(db.is_valid());
  EXPECT_NE(db.native_handle(), nullptr);
  EXPECT_TRUE(fs::exists(db_path));
}

TEST(DatabaseTest, ConstructorCreatesParentDirectories) {
  ai::base::TempDir dir;
  auto db_path =
      (fs::path(dir.path()) / "nested" / "subdir" / "test.db").string();

  EXPECT_FALSE(fs::exists(fs::path(db_path).parent_path()));

  ai::base::database db(db_path);
  EXPECT_TRUE(db.is_valid());
  EXPECT_TRUE(fs::exists(db_path));
}

TEST(DatabaseTest, IsValidAfterConstruction) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "valid.db").string();

  ai::base::database db(db_path);
  EXPECT_TRUE(db.is_valid());
}

TEST(DatabaseTest, NativeHandleReturnsSqlite3Pointer) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "handle.db").string();

  ai::base::database db(db_path);
  sqlite3* handle = db.native_handle();
  EXPECT_NE(handle, nullptr);

  // Verify the handle actually works by executing a simple SQL
  char* err = nullptr;
  int rc = sqlite3_exec(handle, "CREATE TABLE test(id INTEGER);", nullptr,
                        nullptr, &err);
  EXPECT_EQ(rc, SQLITE_OK) << (err ? err : "unknown error");
  if (err) {
    sqlite3_free(err);
  }
}

TEST(DatabaseTest, MoveConstructorTransfersOwnership) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "move.db").string();

  ai::base::database src(db_path);
  EXPECT_TRUE(src.is_valid());
  sqlite3* original_handle = src.native_handle();

  ai::base::database dst(std::move(src));

  // Source should be invalid after move
  EXPECT_FALSE(src.is_valid());
  EXPECT_EQ(src.native_handle(), nullptr);

  // Destination should own the original handle
  EXPECT_TRUE(dst.is_valid());
  EXPECT_EQ(dst.native_handle(), original_handle);
}

TEST(DatabaseTest, MoveAssignmentTransfersOwnership) {
  ai::base::TempDir dir;
  auto db1_path = (fs::path(dir.path()) / "db1.db").string();
  auto db2_path = (fs::path(dir.path()) / "db2.db").string();

  ai::base::database db1(db1_path);
  ai::base::database db2(db2_path);
  EXPECT_TRUE(db1.is_valid());
  EXPECT_TRUE(db2.is_valid());

  sqlite3* db2_handle = db2.native_handle();

  db1 = std::move(db2);

  // db1 should now hold db2's original handle
  EXPECT_TRUE(db1.is_valid());
  EXPECT_EQ(db1.native_handle(), db2_handle);

  // db2 should be invalid
  EXPECT_FALSE(db2.is_valid());
  EXPECT_EQ(db2.native_handle(), nullptr);
}

TEST(DatabaseTest, SelfMoveAssignmentIsSafe) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "selfmove.db").string();

  ai::base::database db(db_path);
  EXPECT_TRUE(db.is_valid());
  sqlite3* handle = db.native_handle();

  // Self-move-assignment should be a no-op
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
  db = std::move(db);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  EXPECT_TRUE(db.is_valid());
  EXPECT_EQ(db.native_handle(), handle);
}

TEST(DatabaseTest, DestructorAllowsReopen) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "reopen.db").string();

  {
    ai::base::database db(db_path);
    EXPECT_TRUE(db.is_valid());

    // Write some data
    char* err = nullptr;
    sqlite3_exec(db.native_handle(),
                 "CREATE TABLE t(x TEXT);"
                 "INSERT INTO t VALUES('hello');",
                 nullptr, nullptr, &err);
    if (err) {
      sqlite3_free(err);
    }
  }
  // db is now destroyed and file should be closed

  // Reopen the same file — should succeed (file not locked)
  ai::base::database db2(db_path);
  EXPECT_TRUE(db2.is_valid());

  // Verify the data persisted
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db2.native_handle(), "SELECT x FROM t;", -1,
                              &stmt, nullptr);
  EXPECT_EQ(rc, SQLITE_OK);
  rc = sqlite3_step(stmt);
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
               "hello");
  sqlite3_finalize(stmt);
}

TEST(DatabaseTest, MultipleDatabasesCanCoexist) {
  ai::base::TempDir dir;
  auto path1 = (fs::path(dir.path()) / "multi1.db").string();
  auto path2 = (fs::path(dir.path()) / "multi2.db").string();

  ai::base::database db1(path1);
  ai::base::database db2(path2);

  EXPECT_TRUE(db1.is_valid());
  EXPECT_TRUE(db2.is_valid());
  EXPECT_NE(db1.native_handle(), db2.native_handle());
  EXPECT_NE(path1, path2);
}

TEST(DatabaseTest, ResetReleasesTheHandle) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "reset.db").string();

  ai::base::database db(db_path);
  EXPECT_TRUE(db.is_valid());

  // We can't call reset() directly since it's private, but we can test
  // via move assignment which internally calls reset().
  ai::base::database other((fs::path(dir.path()) / "other.db").string());
  db = std::move(other);

  EXPECT_TRUE(db.is_valid());
  // The original handle was properly closed during move assignment's reset()
  // (no crash, no leak — verified by sanitizers in CI)
}

// =============================================================================
// database — edge cases
// =============================================================================

TEST(DatabaseTest, MoveConstructedFromDefaultState) {
  // We can't default-construct database, so we simulate by moving from
  // a moved-from database (which is in the null state).
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "movedfrom.db").string();

  ai::base::database src(db_path);
  ai::base::database dst(std::move(src));  // src is now null

  // dst is valid
  EXPECT_TRUE(dst.is_valid());

  // Move from the now-null src
  ai::base::database dst2(std::move(src));
  EXPECT_FALSE(dst2.is_valid());
  EXPECT_EQ(dst2.native_handle(), nullptr);
}

TEST(DatabaseTest, RepeatedMoveAssignmentDoesNotLeak) {
  ai::base::TempDir dir;
  auto p1 = (fs::path(dir.path()) / "r1.db").string();
  auto p2 = (fs::path(dir.path()) / "r2.db").string();
  auto p3 = (fs::path(dir.path()) / "r3.db").string();

  ai::base::database db(p1);

  ai::base::database other1(p2);
  ai::base::database other2(p3);

  sqlite3* h2 = other2.native_handle();

  db = std::move(other1);
  EXPECT_TRUE(db.is_valid());

  db = std::move(other2);
  EXPECT_TRUE(db.is_valid());
  EXPECT_EQ(db.native_handle(), h2);
}

// =============================================================================
// database::exec
// =============================================================================

TEST(DatabaseExecTest, ExecCreateTable) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT);"));

  // Verify the table exists by querying sqlite_master
  char* err = nullptr;
  int count = 0;
  auto callback = [](void* data, int, char**, char**) -> int {
    (*static_cast<int*>(data))++;
    return 0;
  };
  sqlite3_exec(
      db.native_handle(),
      "SELECT name FROM sqlite_master WHERE type='table' AND name='t';",
      callback, &count, &err);
  if (err) {
    sqlite3_free(err);
  }
  EXPECT_EQ(count, 1);
}

TEST(DatabaseExecTest, ExecInsertAndVerify) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('hello');"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('world');"));

  // Verify data via prepared statement
  ai::base::statement stmt(db, "SELECT val FROM t ORDER BY val;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "hello");
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "world");
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(DatabaseExecTest, ExecPragma) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("PRAGMA journal_mode=WAL;"));
  EXPECT_TRUE(db.exec("PRAGMA busy_timeout=5000;"));
  EXPECT_TRUE(db.exec("PRAGMA foreign_keys=ON;"));
}

TEST(DatabaseExecTest, ExecCreateIndex) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(id INTEGER, name TEXT);"));
  EXPECT_TRUE(db.exec("CREATE INDEX IF NOT EXISTS idx_t_name ON t(name);"));
  EXPECT_TRUE(db.exec("CREATE INDEX IF NOT EXISTS idx_t_name ON t(name);"));
}

TEST(DatabaseExecTest, ExecInvalidSqlReturnsFalse) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_FALSE(db.exec("NOT A VALID SQL STATEMENT!!!"));
}

TEST(DatabaseExecTest, ExecOnNonexistentTableReturnsFalse) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_FALSE(db.exec("SELECT * FROM nonexistent_table;"));
}

TEST(DatabaseExecTest, ExecMultipleStatements) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(
      db.exec("CREATE TABLE t(x TEXT);"
              "INSERT INTO t VALUES('a');"
              "INSERT INTO t VALUES('b');"
              "INSERT INTO t VALUES('c');"));

  ai::base::statement stmt(db, "SELECT COUNT(*) FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 3);
}

TEST(DatabaseExecTest, ExecAfterMoveConstructed) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database src(db_path);
  ASSERT_TRUE(src.is_valid());

  ai::base::database dst(std::move(src));
  EXPECT_FALSE(src.is_valid());
  EXPECT_TRUE(dst.is_valid());

  // exec should work on the moved-to database
  EXPECT_TRUE(dst.exec("CREATE TABLE t(x TEXT);"));
  EXPECT_TRUE(dst.exec("INSERT INTO t VALUES('moved');"));

  ai::base::statement stmt(dst, "SELECT x FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "moved");
}

TEST(DatabaseExecTest, ExecReturnsFalseOnMovedFromDatabase) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database src(db_path);
  ASSERT_TRUE(src.is_valid());

  ai::base::database dst(std::move(src));
  EXPECT_FALSE(src.is_valid());

  // exec on a moved-from database should handle nullptr gracefully
  EXPECT_FALSE(src.exec("CREATE TABLE t(x TEXT);"));
}

TEST(DatabaseExecTest, ExecBeginCommitRollback) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));

  // Begin, insert, rollback — data should not persist
  EXPECT_TRUE(db.exec("BEGIN TRANSACTION;"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('rolled_back');"));
  EXPECT_TRUE(db.exec("ROLLBACK;"));

  ai::base::statement stmt(db, "SELECT COUNT(*) FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 0);

  // Begin, insert, commit — data should persist
  EXPECT_TRUE(db.exec("BEGIN TRANSACTION;"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('committed');"));
  EXPECT_TRUE(db.exec("COMMIT;"));

  ai::base::statement stmt2(db, "SELECT val FROM t;");
  ASSERT_TRUE(stmt2.is_valid());
  EXPECT_EQ(stmt2.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt2.get<std::string>(0), "committed");
}

TEST(DatabaseExecTest, ExecEmptyString) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  // sqlite3_exec with empty string should succeed (no-op)
  EXPECT_TRUE(db.exec(""));
}

// =============================================================================
// statement
// =============================================================================

TEST(StatementTest, ConstructorPreparesValidSql) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  // Create a table first
  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_NE(stmt.native_handle(), nullptr);
}

TEST(StatementTest, ConstructorFailsOnInvalidSql) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  ai::base::statement stmt(db, "NOT VALID SQL AT ALL!!!");
  EXPECT_FALSE(stmt.is_valid());
}

TEST(StatementTest, StepReturnsRows) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('hello');"
               "INSERT INTO t VALUES('world');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t ORDER BY x;");
  EXPECT_TRUE(stmt.is_valid());

  // First row
  int rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   sqlite3_column_text(stmt.native_handle(), 0)),
               "hello");

  // Second row
  rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   sqlite3_column_text(stmt.native_handle(), 0)),
               "world");

  // No more rows
  rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_DONE);
}

TEST(StatementTest, BindTextAndStep) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(name TEXT, value TEXT);"
               "INSERT INTO t VALUES('foo', 'bar');"
               "INSERT INTO t VALUES('baz', 'qux');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT value FROM t WHERE name = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  std::string name = "baz";
  sqlite3_bind_text(stmt.native_handle(), 1, name.c_str(),
                    static_cast<int>(name.size()), SQLITE_STATIC);

  int rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   sqlite3_column_text(stmt.native_handle(), 0)),
               "qux");
}

TEST(StatementTest, BindIntAndStep) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(id INTEGER, val TEXT);"
               "INSERT INTO t VALUES(1, 'one');"
               "INSERT INTO t VALUES(2, 'two');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t WHERE id = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  sqlite3_bind_int(stmt.native_handle(), 1, 2);

  int rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   sqlite3_column_text(stmt.native_handle(), 0)),
               "two");
}

TEST(StatementTest, BindNullAndStep) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  int rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_type(stmt.native_handle(), 0), SQLITE_NULL);
}

TEST(StatementTest, InsertThenSelect) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  // Insert via statement
  {
    ai::base::statement stmt(db, "INSERT INTO t VALUES(?1);");
    EXPECT_TRUE(stmt.is_valid());
    std::string val = "inserted_value";
    sqlite3_bind_text(stmt.native_handle(), 1, val.c_str(),
                      static_cast<int>(val.size()), SQLITE_STATIC);
    int rc = sqlite3_step(stmt.native_handle());
    EXPECT_EQ(rc, SQLITE_DONE);
  }

  // Select via statement
  {
    ai::base::statement stmt(db, "SELECT x FROM t;");
    EXPECT_TRUE(stmt.is_valid());
    int rc = sqlite3_step(stmt.native_handle());
    EXPECT_EQ(rc, SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(
                     sqlite3_column_text(stmt.native_handle(), 0)),
                 "inserted_value");
  }
}

TEST(StatementTest, MoveConstructorTransfersOwnership) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement src(db, "SELECT x FROM t;");
  EXPECT_TRUE(src.is_valid());
  sqlite3_stmt* original_handle = src.native_handle();

  ai::base::statement dst(std::move(src));

  // Source should be invalid after move
  EXPECT_FALSE(src.is_valid());
  EXPECT_EQ(src.native_handle(), nullptr);

  // Destination should own the original handle
  EXPECT_TRUE(dst.is_valid());
  EXPECT_EQ(dst.native_handle(), original_handle);
}

TEST(StatementTest, MoveAssignmentTransfersOwnership) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('a');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt1(db, "SELECT x FROM t;");
  ai::base::statement stmt2(db, "SELECT COUNT(*) FROM t;");
  EXPECT_TRUE(stmt1.is_valid());
  EXPECT_TRUE(stmt2.is_valid());

  sqlite3_stmt* stmt2_handle = stmt2.native_handle();

  stmt1 = std::move(stmt2);

  // stmt1 should now hold stmt2's original handle
  EXPECT_TRUE(stmt1.is_valid());
  EXPECT_EQ(stmt1.native_handle(), stmt2_handle);

  // stmt2 should be invalid
  EXPECT_FALSE(stmt2.is_valid());
  EXPECT_EQ(stmt2.native_handle(), nullptr);

  // stmt1 should still work (starts with SELECT COUNT(*))
  int rc = sqlite3_step(stmt1.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(stmt1.native_handle(), 0), 1);
}

TEST(StatementTest, SelfMoveAssignmentIsSafe) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  sqlite3_stmt* handle = stmt.native_handle();

  // Self-move-assignment should be a no-op
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
  stmt = std::move(stmt);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.native_handle(), handle);
}

TEST(StatementTest, DestructorAllowsReprepare) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('hello');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  // First statement — out of scope, destructor should finalize
  {
    ai::base::statement stmt(db, "SELECT x FROM t;");
    EXPECT_TRUE(stmt.is_valid());
    int rc = sqlite3_step(stmt.native_handle());
    EXPECT_EQ(rc, SQLITE_ROW);
  }

  // Second statement on the same db — should work (previous was finalized)
  {
    ai::base::statement stmt2(db, "SELECT x FROM t;");
    EXPECT_TRUE(stmt2.is_valid());
    int rc = sqlite3_step(stmt2.native_handle());
    EXPECT_EQ(rc, SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(
                     sqlite3_column_text(stmt2.native_handle(), 0)),
                 "hello");
  }
}

TEST(StatementTest, ManyStatementsNoResourceExhaustion) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  // Create and destroy many statements — should not exhaust resources
  // since each destructor calls sqlite3_finalize
  for (int i = 0; i < 100; ++i) {
    ai::base::statement stmt(db, "SELECT x FROM t;");
    EXPECT_TRUE(stmt.is_valid());
    sqlite3_step(stmt.native_handle());  // SQLITE_DONE
  }
  // If we get here without crash/resource issue, the destructor works
  SUCCEED();
}

TEST(StatementTest, ColumnCountAndTypes) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(a INTEGER, b TEXT, c REAL);"
               "INSERT INTO t VALUES(42, 'text', 3.14);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT a, b, c FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  int rc = sqlite3_step(stmt.native_handle());
  EXPECT_EQ(rc, SQLITE_ROW);

  EXPECT_EQ(sqlite3_column_count(stmt.native_handle()), 3);
  EXPECT_EQ(sqlite3_column_int(stmt.native_handle(), 0), 42);
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   sqlite3_column_text(stmt.native_handle(), 1)),
               "text");
  EXPECT_DOUBLE_EQ(sqlite3_column_double(stmt.native_handle(), 2), 3.14);
}

// =============================================================================
// statement::bind
// =============================================================================

TEST(StatementBindTest, BindNull) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('hello');"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t WHERE x IS ?1;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind(1, nullptr));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindInt) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(id INTEGER, val TEXT);"
               "INSERT INTO t VALUES(1, 'one');"
               "INSERT INTO t VALUES(2, 'two');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t WHERE id = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind(1, 2));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindInt64) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(id INTEGER, val TEXT);"
               "INSERT INTO t VALUES(10000000000, 'large');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t WHERE id = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind(1, static_cast<sqlite3_int64>(10000000000)));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindDouble) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val REAL);"
               "INSERT INTO t VALUES(3.14);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t WHERE val > ?1;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind(1, 3.0));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindStringView) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(name TEXT);"
               "INSERT INTO t VALUES('alice');"
               "INSERT INTO t VALUES('bob');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT name FROM t WHERE name = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind(1, std::string_view{"bob"}));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindStringLiteral) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(name TEXT);"
               "INSERT INTO t VALUES('hello');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT name FROM t WHERE name = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  // std::string literal implicitly converts to std::string_view
  EXPECT_TRUE(stmt.bind(1, "hello"));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindStdString) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(name TEXT);"
               "INSERT INTO t VALUES('world');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT name FROM t WHERE name = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  std::string value = "world";
  EXPECT_TRUE(stmt.bind(1, value));  // std::string → std::string_view
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindTest, BindReturnsFalseOnInvalidIndex) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t WHERE x = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  // Index 0 is out of range (1-based)
  EXPECT_FALSE(stmt.bind(0, 42));
}

// =============================================================================
// statement::bind_all
// =============================================================================

TEST(StatementBindAllTest, BindsMultipleParams) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(a INTEGER, b TEXT, c REAL);"
               "INSERT INTO t VALUES(1, 'one', 1.1);"
               "INSERT INTO t VALUES(2, 'two', 2.2);"
               "INSERT INTO t VALUES(3, 'three', 3.3);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT b FROM t WHERE a = ?1 AND c > ?2;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind_all(2, 2.0));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindAllTest, BindsMixedTypes) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(a INTEGER, b TEXT, c REAL);"
               "INSERT INTO t VALUES(42, 'answer', 3.14);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(
      db, "SELECT a FROM t WHERE a = ?1 AND b = ?2 AND c = ?3;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind_all(42, std::string_view{"answer"}, 3.14));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindAllTest, BindsNullInMiddle) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(a INTEGER, b TEXT);"
               "INSERT INTO t VALUES(1, 'hello');"
               "INSERT INTO t VALUES(2, NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT a FROM t WHERE a = ?1 AND b IS ?2;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_TRUE(stmt.bind_all(2, nullptr));
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementBindAllTest, ShortCircuitsOnFirstFailure) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t WHERE x = ?1;");
  EXPECT_TRUE(stmt.is_valid());

  // Index 0 is invalid — bind_all returns false immediately
  EXPECT_FALSE(stmt.bind_all(0, "value", 3.14));
}

// =============================================================================
// statement::step (step_result enum)
// =============================================================================

TEST(StatementStepTest, ReturnsRowWhenDataAvailable) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('hello');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
}

TEST(StatementStepTest, ReturnsDoneWhenNoMoreRows) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  // No rows in table
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(StatementStepTest, ReturnsDoneAfterLastRow) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('a');"
               "INSERT INTO t VALUES('b');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(StatementStepTest, InsertReturnsDone) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "INSERT INTO t VALUES('hello');");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(StatementStepTest, ErrorOnConstraintViolation) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(id INTEGER PRIMARY KEY);"
               "INSERT INTO t VALUES(1);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "INSERT INTO t VALUES(?1);");
  EXPECT_TRUE(stmt.is_valid());

  // Insert duplicate primary key — should return error
  EXPECT_TRUE(stmt.bind(1, 1));
  EXPECT_EQ(stmt.step(), ai::base::step_result::error);
}

TEST(StatementStepTest, StepAfterDoneReturnsDone) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(), "CREATE TABLE t(x TEXT);", nullptr, nullptr,
               &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t;");
  EXPECT_TRUE(stmt.is_valid());

  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
  // Stepping again after done is safe and continues to return done
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(StatementStepTest, WhileLoopPattern) {
  // Simulates the pattern used in init_db() and list_sessions():
  //   while (stmt.step() == step_result::row) { ... }
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(x TEXT);"
               "INSERT INTO t VALUES('a');"
               "INSERT INTO t VALUES('b');"
               "INSERT INTO t VALUES('c');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT x FROM t ORDER BY x;");
  EXPECT_TRUE(stmt.is_valid());

  int count = 0;
  while (stmt.step() == ai::base::step_result::row) {
    ++count;
  }
  EXPECT_EQ(count, 3);
}

TEST(StatementStepTest, BindAllAndStepThenGet) {
  // Simulates the pattern used in get_messages():
  //   stmt.bind(1, ...); stmt.step(); stmt.get<>(0);
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(id INTEGER, name TEXT, val REAL);"
               "INSERT INTO t VALUES(1, 'alice', 1.1);"
               "INSERT INTO t VALUES(2, 'bob', 2.2);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  // bind_all + step + get (multiple columns)
  {
    ai::base::statement stmt(db, "SELECT name, val FROM t WHERE id = ?1;");
    EXPECT_TRUE(stmt.is_valid());

    EXPECT_TRUE(stmt.bind_all(2));
    EXPECT_EQ(stmt.step(), ai::base::step_result::row);
    EXPECT_EQ(stmt.get<std::string>(0), "bob");
    EXPECT_DOUBLE_EQ(stmt.get<double>(1), 2.2);
  }

  // bind individually + step + get
  {
    ai::base::statement stmt(
        db, "SELECT id, name FROM t WHERE val > ?1 ORDER BY id;");
    EXPECT_TRUE(stmt.is_valid());

    EXPECT_TRUE(stmt.bind(1, 1.0));

    EXPECT_EQ(stmt.step(), ai::base::step_result::row);
    EXPECT_EQ(stmt.get<int>(0), 1);
    EXPECT_EQ(stmt.get<std::string>(1), "alice");

    EXPECT_EQ(stmt.step(), ai::base::step_result::row);
    EXPECT_EQ(stmt.get<int>(0), 2);
    EXPECT_EQ(stmt.get<std::string>(1), "bob");

    EXPECT_EQ(stmt.step(), ai::base::step_result::done);
  }
}

// =============================================================================
// statement::get
// =============================================================================

TEST(StatementGetTest, GetInt) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(42);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 42);
}

TEST(StatementGetTest, GetInt64) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(9223372036854775807);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<sqlite3_int64>(0), 9223372036854775807);
}

TEST(StatementGetTest, GetDouble) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val REAL);"
               "INSERT INTO t VALUES(3.14159);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_DOUBLE_EQ(stmt.get<double>(0), 3.14159);
}

TEST(StatementGetTest, GetString) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val TEXT);"
               "INSERT INTO t VALUES('hello world');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "hello world");
}

TEST(StatementGetTest, GetOptionalSome) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(99);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<int>>(0);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, 99);
}

TEST(StatementGetTest, GetOptionalNone) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val TEXT);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<std::string>>(0);
  EXPECT_FALSE(result.has_value());
}

TEST(StatementGetTest, GetStringOnNullReturnsEmpty) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val TEXT);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_TRUE(stmt.get<std::string>(0).empty());
}

TEST(StatementGetTest, GetStringOnEmptyString) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val TEXT);"
               "INSERT INTO t VALUES('');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_TRUE(stmt.get<std::string>(0).empty());
}

TEST(StatementGetTest, GetIntOnNullReturnsZero) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 0);
}

TEST(StatementGetTest, GetInt64OnNullReturnsZero) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<sqlite3_int64>(0), 0);
}

TEST(StatementGetTest, GetDoubleOnNullReturnsZero) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val REAL);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_DOUBLE_EQ(stmt.get<double>(0), 0.0);
}

TEST(StatementGetTest, GetOptionalIntNone) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<int>>(0);
  EXPECT_FALSE(result.has_value());
}

TEST(StatementGetTest, GetOptionalInt64Some) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(9223372036854775807);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<sqlite3_int64>>(0);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, 9223372036854775807);
}

TEST(StatementGetTest, GetOptionalInt64None) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val INTEGER);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<sqlite3_int64>>(0);
  EXPECT_FALSE(result.has_value());
}

TEST(StatementGetTest, GetOptionalDoubleSome) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val REAL);"
               "INSERT INTO t VALUES(3.14159);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<double>>(0);
  EXPECT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(*result, 3.14159);
}

TEST(StatementGetTest, GetOptionalDoubleNone) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val REAL);"
               "INSERT INTO t VALUES(NULL);",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<double>>(0);
  EXPECT_FALSE(result.has_value());
}

TEST(StatementGetTest, GetOptionalStringSome) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);

  char* err = nullptr;
  sqlite3_exec(db.native_handle(),
               "CREATE TABLE t(val TEXT);"
               "INSERT INTO t VALUES('hello world');",
               nullptr, nullptr, &err);
  if (err) {
    sqlite3_free(err);
  }

  ai::base::statement stmt(db, "SELECT val FROM t;");
  EXPECT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  auto result = stmt.get<std::optional<std::string>>(0);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, "hello world");
}

// =============================================================================
// transaction
// =============================================================================

TEST(TransactionTest, CommitPersistsChanges) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));

  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('committed');"));
    tx.commit();
  }

  // Verify data persisted after commit
  ai::base::statement stmt(db, "SELECT val FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "committed");
}

TEST(TransactionTest, DestructorRollsBackUncommitted) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));

  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('rolled_back');"));
    // tx goes out of scope without commit → ROLLBACK in destructor
  }

  // Verify data was rolled back
  ai::base::statement stmt(db, "SELECT COUNT(*) FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 0);
}

TEST(TransactionTest, DestructorAfterCommitIsSafe) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('before');"));

  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('during');"));
    tx.commit();
    // Destructor should not issue ROLLBACK since committed_ is true
  }

  // Verify both rows exist (no spurious rollback)
  ai::base::statement stmt(db, "SELECT COUNT(*) FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 2);
}

TEST(TransactionTest, DoubleCommitIsSafe) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));

  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('once');"));
    tx.commit();
    tx.commit();  // Second commit should be a no-op
  }

  // Data should still be there (second commit didn't break anything)
  ai::base::statement stmt(db, "SELECT val FROM t;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "once");
}

TEST(TransactionTest, MultipleSeparateTransactions) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));

  // First transaction: committed
  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('first');"));
    tx.commit();
  }

  // Second transaction: rolled back
  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('second_rolled_back');"));
    // no commit → rollback
  }

  // Third transaction: committed
  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('third');"));
    tx.commit();
  }

  // Only first and third should persist
  ai::base::statement stmt(db, "SELECT val FROM t ORDER BY val;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "first");
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "third");
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(TransactionTest, RollbackPreservesPreTransactionState) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, val TEXT);"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES(1, 'original');"));

  {
    ai::base::transaction tx(db);
    // Modify existing row and insert new row
    EXPECT_TRUE(db.exec("UPDATE t SET val = 'modified' WHERE id = 1;"));
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES(2, 'new_row');"));
    // no commit → rollback
  }

  // Original data should be intact, new row should not exist
  ai::base::statement stmt(db, "SELECT id, val FROM t ORDER BY id;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<int>(0), 1);
  EXPECT_EQ(stmt.get<std::string>(1), "original");
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(TransactionTest, TransactionWithPreparedStatements) {
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(name TEXT, value INTEGER);"));

  {
    ai::base::transaction tx(db);

    {
      ai::base::statement stmt(db, "INSERT INTO t VALUES(?1, ?2);");
      ASSERT_TRUE(stmt.is_valid());
      EXPECT_TRUE(stmt.bind_all(std::string_view{"alice"}, 100));
      EXPECT_EQ(stmt.step(), ai::base::step_result::done);
    }
    {
      ai::base::statement stmt(db, "INSERT INTO t VALUES(?1, ?2);");
      ASSERT_TRUE(stmt.is_valid());
      EXPECT_TRUE(stmt.bind_all(std::string_view{"bob"}, 200));
      EXPECT_EQ(stmt.step(), ai::base::step_result::done);
    }

    tx.commit();
  }

  ai::base::statement stmt(db, "SELECT name, value FROM t ORDER BY name;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "alice");
  EXPECT_EQ(stmt.get<int>(1), 100);
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "bob");
  EXPECT_EQ(stmt.get<int>(1), 200);
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}

TEST(TransactionTest, CommitThenRollbackInSeparateTransaction) {
  // Verify that commit in one transaction doesn't affect another transaction's
  // rollback
  ai::base::TempDir dir;
  auto db_path = (fs::path(dir.path()) / "test.db").string();
  ai::base::database db(db_path);
  ASSERT_TRUE(db.is_valid());

  EXPECT_TRUE(db.exec("CREATE TABLE t(val TEXT);"));
  EXPECT_TRUE(db.exec("INSERT INTO t VALUES('baseline');"));

  // Transaction A: commit
  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('from_a');"));
    tx.commit();
  }

  // Transaction B: rollback
  {
    ai::base::transaction tx(db);
    EXPECT_TRUE(db.exec("INSERT INTO t VALUES('from_b_rolled_back');"));
    // no commit → rollback
  }

  // Only baseline and from_a should exist
  ai::base::statement stmt(db, "SELECT val FROM t ORDER BY val;");
  ASSERT_TRUE(stmt.is_valid());
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "baseline");
  EXPECT_EQ(stmt.step(), ai::base::step_result::row);
  EXPECT_EQ(stmt.get<std::string>(0), "from_a");
  EXPECT_EQ(stmt.step(), ai::base::step_result::done);
}
