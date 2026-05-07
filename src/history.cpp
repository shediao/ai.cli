#include "ai/history.h"

#include <sqlite3.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <thread>

#include "ai/args.h"
#include "ai/logging.h"
#include "ai/openai.h"
#include "ai/utils.h"
#include "nlohmann/json_fwd.hpp"

namespace ai {

// ── helpers ────────────────────────────────────────────────────────────────

namespace {

/// Execute @p sql that returns no rows.
bool exec_sql(sqlite3* db, std::string const& sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "SQL error: " << (err_msg ? err_msg : "unknown")
               << " (sql=" << sql << ")";
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

/// Generate a random hex string of length @p len bytes (produces 2*len chars).
std::string random_hex(size_t len) {
  static thread_local std::mt19937_64 rng(
      std::chrono::steady_clock::now().time_since_epoch().count() ^
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  static const char hex_chars[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  std::uniform_int_distribution<uint32_t> dist(0, 255);
  for (size_t i = 0; i < len; ++i) {
    auto byte = static_cast<uint8_t>(dist(rng));
    out.push_back(hex_chars[byte >> 4]);
    out.push_back(hex_chars[byte & 0x0F]);
  }
  return out;
}

}  // namespace

// ── HistoryDB ──────────────────────────────────────────────────────────────

HistoryDB::HistoryDB(std::string db_path) : db_path_(std::move(db_path)) {
  // Ensure parent directory exists
  std::filesystem::path p(db_path_);
  if (!p.parent_path().empty() && !std::filesystem::exists(p.parent_path())) {
    std::filesystem::create_directories(p.parent_path());
  }

  int rc = sqlite3_open_v2(
      db_path_.c_str(), &db_,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (rc != SQLITE_OK) {
    LOG(FATAL) << "Failed to open history database " << db_path_ << ": "
               << sqlite3_errmsg(db_);
    sqlite3_close(db_);
    db_ = nullptr;
    return;
  }
  init_db();
}

HistoryDB::~HistoryDB() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void HistoryDB::init_db() {
  if (!db_) {
    return;
  }

  // Enable WAL mode for concurrent multi-process access
  exec_sql(db_, "PRAGMA journal_mode=WAL;");

  // Set busy timeout to 5 seconds so we wait if another process is writing
  exec_sql(db_, "PRAGMA busy_timeout=5000;");

  // Enable foreign keys (good practice; not strictly needed for single table)
  exec_sql(db_, "PRAGMA foreign_keys=ON;");

  // Create the conversations table
  char* err_msg = nullptr;
  const char* create_sql = R"SQL(
    CREATE TABLE IF NOT EXISTS conversations (
      id          INTEGER PRIMARY KEY AUTOINCREMENT,
      session_id  TEXT    NOT NULL UNIQUE,
      created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
      updated_at  TEXT    NOT NULL DEFAULT (datetime('now')),
      topic       TEXT    NOT NULL DEFAULT '',
      messages    TEXT    NOT NULL
    );
  )SQL";
  int rc = sqlite3_exec(db_, create_sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to create conversations table: "
               << (err_msg ? err_msg : "unknown");
    sqlite3_free(err_msg);
    return;
  }

  // Migration: add topic column if upgrading from older schema
  // Check if the column already exists before attempting to add it
  {
    sqlite3_stmt* stmt = nullptr;
    bool has_topic = false;
    int rc = sqlite3_prepare_v2(db_, "PRAGMA table_info(conversations);", -1,
                                &stmt, nullptr);
    if (rc == SQLITE_OK) {
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto const* col_name =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (col_name && std::strcmp(col_name, "topic") == 0) {
          has_topic = true;
          break;
        }
      }
    }
    sqlite3_finalize(stmt);
    if (!has_topic) {
      exec_sql(db_,
               "ALTER TABLE conversations ADD COLUMN topic TEXT NOT NULL "
               "DEFAULT '';");
    }
  }

  // Create index for ordering by updated_at
  exec_sql(db_,
           "CREATE INDEX IF NOT EXISTS idx_conversations_updated_at "
           "ON conversations(updated_at);");

  LOG(INFO) << "History database initialized: " << db_path_;
}

std::string HistoryDB::generate_session_id() const {
  // Format: "YYYYMMDD-HHMMSS-<16hex>"
  std::ostringstream oss;
  oss << ai::utils::timestamp("%Y%m%d-%H%M%S");
  oss << '-' << random_hex(8);
  return oss.str();
}

std::string HistoryDB::create_session() {
  if (!db_) {
    return "";
  }

  std::string session_id = generate_session_id();

  // Insert an empty messages row
  const char* insert_sql =
      "INSERT INTO conversations (session_id, messages) VALUES (?1, ?2);";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare insert: " << sqlite3_errmsg(db_);
    return "";
  }

  sqlite3_bind_text(stmt, 1, session_id.c_str(),
                    static_cast<int>(session_id.size()), SQLITE_STATIC);

  nlohmann::json empty_array = nlohmann::json::array();
  std::string empty_json = empty_array.dump();
  sqlite3_bind_text(stmt, 2, empty_json.c_str(),
                    static_cast<int>(empty_json.size()), SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    LOG(ERROR) << "Failed to create session: " << sqlite3_errmsg(db_);
    return "";
  }

  LOG(INFO) << "Created session: " << session_id;
  return session_id;
}

std::optional<nlohmann::json> HistoryDB::get_messages(
    std::string const& session_id) {
  if (!db_) {
    return std::nullopt;
  }

  const char* select_sql =
      "SELECT messages FROM conversations WHERE session_id = ?1;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare select: " << sqlite3_errmsg(db_);
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, session_id.c_str(),
                    static_cast<int>(session_id.size()), SQLITE_STATIC);

  std::optional<nlohmann::json> result;
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    auto const* text =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    int text_len = sqlite3_column_bytes(stmt, 0);
    if (text && text_len > 0) {
      try {
        result = nlohmann::json::parse(std::string_view(text, text_len));
      } catch (nlohmann::json::parse_error const& e) {
        LOG(ERROR) << "Failed to parse messages JSON: " << e.what();
      }
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

void HistoryDB::save_messages(std::string const& session_id,
                              nlohmann::json const& messages) {
  if (!db_) {
    return;
  }
  if (!messages.is_array() || messages.empty()) {
    return;
  }

  // Wrap in a transaction for atomicity and performance
  exec_sql(db_, "BEGIN TRANSACTION;");

  const char* upsert_sql = R"SQL(
    INSERT INTO conversations (session_id, updated_at, messages)
    VALUES (?1, datetime('now'), ?2)
    ON CONFLICT(session_id) DO UPDATE SET
      updated_at = excluded.updated_at,
      messages   = excluded.messages;
  )SQL";

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, upsert_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare upsert: " << sqlite3_errmsg(db_);
    exec_sql(db_, "ROLLBACK;");
    return;
  }

  sqlite3_bind_text(stmt, 1, session_id.c_str(),
                    static_cast<int>(session_id.size()), SQLITE_STATIC);

  std::string messages_json = messages.dump();
  sqlite3_bind_text(stmt, 2, messages_json.c_str(),
                    static_cast<int>(messages_json.size()), SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc == SQLITE_DONE) {
    exec_sql(db_, "COMMIT;");
    LOG(INFO) << "Saved messages " << session_id << " (" << messages.size()
              << " messages)";
  } else {
    LOG(ERROR) << "Failed to save messages: " << sqlite3_errmsg(db_);
    exec_sql(db_, "ROLLBACK;");
  }
}

std::vector<std::string> HistoryDB::list_sessions() {
  std::vector<std::string> sessions;
  if (!db_) {
    return sessions;
  }

  const char* select_sql =
      "SELECT session_id FROM conversations ORDER BY updated_at DESC;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare select: " << sqlite3_errmsg(db_);
    return sessions;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto const* text =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (text) {
      sessions.emplace_back(text);
    }
  }

  sqlite3_finalize(stmt);
  return sessions;
}

std::vector<HistoryDB::SessionInfo> HistoryDB::list_session_infos(int N) {
  std::vector<SessionInfo> infos;
  if (!db_) {
    return infos;
  }

  std::string sql =
      "SELECT session_id, created_at, updated_at, topic, messages FROM "
      "conversations "
      "ORDER BY updated_at DESC";
  if (N > 0) {
    sql += " LIMIT ?1";
  }
  sql += ";";

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare select: " << sqlite3_errmsg(db_);
    return infos;
  }

  if (N > 0) {
    sqlite3_bind_int(stmt, 1, N);
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SessionInfo info;
    if (auto const* t =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) {
      info.session_id = t;
    }
    if (auto const* t =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) {
      info.created_at = t;
    }
    if (auto const* t =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      info.updated_at = t;
    }
    if (auto const* t =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      info.topic = t;
    }
    if (auto const* t =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))) {
      info.messages = t;
    }
    infos.push_back(std::move(info));
  }

  sqlite3_finalize(stmt);
  return infos;
}

void HistoryDB::set_topic(std::string const& session_id,
                          std::string const& topic) {
  if (!db_) {
    return;
  }

  const char* update_sql =
      "UPDATE conversations SET topic = ?1 WHERE session_id = ?2;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare topic update: " << sqlite3_errmsg(db_);
    return;
  }

  sqlite3_bind_text(stmt, 1, topic.c_str(), static_cast<int>(topic.size()),
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, session_id.c_str(),
                    static_cast<int>(session_id.size()), SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    LOG(ERROR) << "Failed to set topic: " << sqlite3_errmsg(db_);
  } else {
    LOG(INFO) << "Set topic for session " << session_id << ": " << topic;
  }
}

std::string HistoryDB::generate_topic(nlohmann::json const& messages) {
  if (!messages.is_array() || messages.empty()) {
    return "";
  }

  // Extract user and assistant messages for topic generation
  std::string conversation_text;
  int message_count = 0;
  for (auto const& msg : messages) {
    if (!msg.is_object() || !msg.contains("role")) {
      continue;
    }
    auto const& role = msg["role"].get<std::string>();
    if (role != "user" && role != "assistant") {
      continue;
    }
    std::string content;
    if (msg.contains("content") && msg["content"].is_string()) {
      content = msg["content"].get<std::string>();
    }
    if (content.empty()) {
      continue;
    }
    // Truncate each message to keep total reasonable
    if (content.size() > 1024) {
      content = content.substr(0, 1021) + "...";
    }
    conversation_text += role + ": " + content + "\n";
    ++message_count;
    // Limit to first ~10 messages for topic generation
    if (message_count >= 10) {
      break;
    }
  }

  if (conversation_text.empty()) {
    return "";
  }

  // Truncate total text to avoid overly long prompts
  if (conversation_text.size() > 8192) {
    conversation_text = conversation_text.substr(0, 8189) + "...";
  }

  try {
    OpenAIClient client;
    nlohmann::json temp_history = nlohmann::json::array();
    std::vector<std::string> user_prompts = {conversation_text};

    auto response = client.chat(
        "You are a topic generator. Summarize the following conversation into "
        "a single brief topic phrase, within 15 words. Output ONLY the topic "
        "text, nothing else - no quotes, no prefixes, no explanation.",
        user_prompts, temp_history);

    if (response.has_value() && !response.value().choices().empty()) {
      auto const& content = response.value().choices().back().message.content;
      if (!content.empty()) {
        // Clean up: remove quotes, newlines, extra whitespace
        std::string topic = content;
        // Remove surrounding quotes
        if (topic.size() >= 2 &&
            ((topic.front() == '"' && topic.back() == '"') ||
             (topic.front() == '\'' && topic.back() == '\''))) {
          topic = topic.substr(1, topic.size() - 2);
        }
        // Trim whitespace
        auto start = topic.find_first_not_of(" \t\n\r");
        auto end = topic.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
          topic = topic.substr(start, end - start + 1);
        }
        return topic;
      }
    }
  } catch (std::exception const& e) {
    LOG(ERROR) << "Topic generation failed: " << e.what();
  }

  return "";
}

void HistoryDB::SessionInfo::print(bool json_format) const {
  try {
    auto msg = nlohmann::json::parse(messages);
    if (!msg.is_array()) {
      return;
    }
    if (json_format) {
      nlohmann::json session;
      session["messages"] = msg;
      session["created_at"] = created_at;
      session["updated_at"] = updated_at;
      session["topic"] = topic;
      std::cout << session.dump();
      return;
    }
    std::cout << "\n================  <" << created_at << ">-<" << updated_at
              << ">";
    if (!topic.empty()) {
      std::cout << "  [" << topic << "]";
    }
    std::cout << "\n";
    for (auto it = msg.begin(); it != msg.end(); it++) {
      auto const& m = *it;
      if (!m.is_object()) {
        continue;
      }
      if (m.contains("role")) {
        std::cout << "\n"
                  << std::distance(msg.begin(), it) + 1 << ". <"
                  << m["role"].get<std::string>() << ">\n";
      }
      if (m.contains("reasoning_content")) {
        std::cout << "\n\n<thinking>\n"
                  << m["reasoning_content"].get<std::string>()
                  << "\n</thinking>\n\n";
      }
      if (m.contains("content")) {
        std::cout << m["content"].get<std::string>() << "\n\n";
      }
      if (m.contains("tool_calls") && m["tool_calls"].is_array()) {
        for (auto const& f : m["tool_calls"]) {
          if (f.contains("function") && f["function"].is_object() &&
              f["function"].contains("name") &&
              f["function"].contains("arguments")) {
            std::cout << "[" << f["function"]["name"].get<std::string>() << "] "
                      << f["function"]["arguments"].get<std::string>()
                      << "\n\n";
          }
        }
      }
    }
  } catch (...) {
  }
}

std::string HistoryDB::default_db_path() {
  return (std::filesystem::path(ai::utils::app_data_dir("ai.cli")) /
          "chat_history.db")
      .string();
}

int history() {
  try {
    AiArgs const& args = AiArgs::instance();
    HistoryDB history_db(HistoryDB::default_db_path());
    int n = args.history_args.n;
    auto sessions = history_db.list_session_infos(n > 0 ? n : -1);

    if (sessions.empty()) {
      if (args.history_args.format == "json") {
        std::cout << "[]\n";
      } else {
        std::cout << "No chat history found.\n";
      }
      return 0;
    }

    if (args.history_args.format == "json") {
      std::cout << "[";
      auto it = sessions.begin();
      it->print(true);
      for (; it != sessions.end(); it++) {
        std::cout << "  ,";
        it->print(true);
      }
      std::cout << "]";
    } else {
      // list_session_infos returns newest-first; reverse to print oldest-first
      for (auto it = sessions.rbegin(); it != sessions.rend(); it++) {
        it->print(false);
      }
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

}  // namespace ai
