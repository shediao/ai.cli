#pragma once

#include <memory>
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
///   auto msgs = db.get_last_messages();              // resume last one
///   db.save_messages(session_id, messages);          // persist
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

  /// Retrieve the most recently updated messages.
  /// @return The messages JSON array, or std::nullopt if the database is empty.
  std::optional<nlohmann::json> get_last_messages();

  /// Retrieve specific messages by session_id.
  /// @return The messages JSON array, or std::nullopt if not found.
  std::optional<nlohmann::json> get_messages(std::string const& session_id);

  /// Save (insert or update) messages for a session.
  /// Automatically updates the `updated_at` timestamp.
  void save_messages(std::string const& session_id,
                     nlohmann::json const& messages);

  /// List all saved session IDs, ordered by most recent first.
  std::vector<std::string> list_sessions();

  /// Per-session metadata.
  struct SessionInfo {
    std::string session_id;
    std::string created_at;
    std::string updated_at;
  };

  /// List all sessions with metadata, ordered by most recent first.
  std::vector<SessionInfo> list_session_infos();

 private:
  void init_db();
  std::string generate_session_id() const;

  std::string db_path_;
  sqlite3* db_{nullptr};
};

}  // namespace ai
