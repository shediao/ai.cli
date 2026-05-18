#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// Forward declare sqlite3
struct sqlite3;

namespace ai {

struct AiArgs;

/// SQLite-backed chat history database.
///
/// Supports concurrent access from multiple processes via WAL journal mode.
/// Each conversation is stored as a row keyed by a unique session_id.
///
/// Uses table `conversations_v1` with Unix-timestamp time fields.
/// The legacy `conversations` table is left untouched for older versions.
///
/// Usage:
///   HistoryDB db("/path/to/chat_history.db");
///   std::string session_id = db.create_session(messages);  // create + save
///   auto last = db.list_session_infos(1);                  // get last session
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

  /// Create a new session and persist its messages in one step.
  /// @param messages The conversation messages JSON array.
  /// @param url The API base URL used for this session.
  /// @param model The AI model name used for this session.
  /// @param work_dir The working directory when the session was created.
  /// @param parent_id The session_id of the previous session this continues
  ///                  from, or empty if this is a fresh session.
  /// @param start_ts Unix timestamp when the chat started (0 = use current
  ///                 time).
  /// @param end_ts Unix timestamp when the chat ended (0 = use current time).
  /// @return A unique session_id string.
  std::string create_session(
      nlohmann::json const& messages = nlohmann::json::array(),
      std::string const& url = "", std::string const& model = "",
      std::string const& work_dir = "", std::string const& parent_id = "",
      int64_t start_ts = 0, int64_t end_ts = 0);

  /// Retrieve specific messages by session_id.
  /// @return The messages JSON array, or std::nullopt if not found.
  std::optional<nlohmann::json> get_messages(std::string const& session_id);

  /// List all saved session IDs, ordered by most recent first.
  std::vector<std::string> list_sessions();

  /// Get the default database path (in ai.cli app data directory).
  static std::string default_db_path();

  /// Per-session metadata.
  struct SessionInfo {
    std::string session_id;
    int64_t start = 0;  ///< Unix timestamp in seconds (session start).
    int64_t end = 0;    ///< Unix timestamp in seconds (session end).
    std::string topic;
    std::string url;
    std::string model;
    std::string work_dir;
    std::string parent_id;
    std::string messages;
    void print(bool json_format) const;
  };

  /// Set the topic for a session.
  void set_topic(std::string const& session_id, std::string const& topic);

  /// Generate a one-sentence topic from conversation messages using AI.
  /// @param messages The conversation messages JSON array.
  /// @return A brief topic string, or empty string on failure.
  static std::string generate_topic(nlohmann::json const& messages,
                                    AiArgs const& args);

  /// List sessions with metadata, ordered by most recent first.
  /// @param N Maximum number of sessions to return. -1 (default) returns all.
  std::vector<SessionInfo> list_session_infos(int N = -1);

  /// Get a single session by ID.
  /// @return SessionInfo if found, std::nullopt otherwise.
  std::optional<SessionInfo> get_session_info(std::string const& session_id);

 private:
  void init_db();
  std::string generate_session_id() const;

  std::string db_path_;
  sqlite3* db_{nullptr};
};

int history(AiArgs const& args);

}  // namespace ai
