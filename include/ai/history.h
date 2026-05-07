#pragma once

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <vector>

// Forward declare sqlite3
struct sqlite3;

namespace ai {

/// SQLite-backed chat history database.
///
/// Supports concurrent access from multiple processes via WAL journal mode.
/// Each conversation is stored as a row keyed by a unique session_id.
///
/// Usage:
///   HistoryDB db("/path/to/chat_history.db");
///   std::string session_id = db.create_session();        // new session
///   db.save_messages(session_id, messages);          // persist
///   auto last = db.list_session_infos(1);            // get last session
class HistoryDB {
 public:
  /// Open (or create) the database at @p db_path.
  /// Enables WAL mode and sets a busy timeout for multi-process safety.
  explicit HistoryDB(std::string db_path);
  ~HistoryDB();

  HistoryDB(HistoryDB const&) = delete;
  HistoryDB& operator=(HistoryDB const&) = delete;
  HistoryDB(HistoryDB&&) = delete;
  HistoryDB& operator=(HistoryDB&&) = delete;

  /// Create a new session.
  /// @return A unique session_id string.
  std::string create_session();

  /// Retrieve specific messages by session_id.
  /// @return The messages JSON array, or std::nullopt if not found.
  std::optional<nlohmann::json> get_messages(std::string const& session_id);

  /// Save (insert or update) messages for a session.
  /// Automatically updates the `updated_at` timestamp.
  void save_messages(std::string const& session_id,
                     nlohmann::json const& messages);

  /// List all saved session IDs, ordered by most recent first.
  std::vector<std::string> list_sessions();

  /// Get the default database path (in ai.cli app data directory).
  static std::string default_db_path();

  /// Per-session metadata.
  struct SessionInfo {
    std::string session_id;
    std::string created_at;
    std::string updated_at;
    std::string messages;
    void print(bool json_format) const;
  };

  /// List sessions with metadata, ordered by most recent first.
  /// @param N Maximum number of sessions to return. -1 (default) returns all.
  std::vector<SessionInfo> list_session_infos(int N = -1);

 private:
  void init_db();
  std::string generate_session_id() const;

  std::string db_path_;
  sqlite3* db_{nullptr};
};

int history();

}  // namespace ai
