
#include "database.h"

#include <filesystem>
#include <utility>

#include "logging.h"

namespace ai::base {

database::database(std::string_view path) {
  std::filesystem::path p(path);
  if (!p.parent_path().empty() && !std::filesystem::exists(p.parent_path())) {
    std::filesystem::create_directories(p.parent_path());
  }

  sqlite3* db = nullptr;
  int rc = sqlite3_open_v2(
      path.data(), &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to open sqlite database " << path << ": "
               << sqlite3_errmsg(db);
    sqlite3_close(db);
    db = nullptr;
    return;
  }
  db_ = db;
}
database::database(database&& other) noexcept
    : db_{std::exchange(other.db_, nullptr)} {}
database& database::operator=(database&& other) noexcept {
  if (this != &other) {
    reset();
    db_ = std::exchange(other.db_, nullptr);
  }
  return *this;
}

database::~database() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void database::reset() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool database::exec(std::string const& sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "SQL error: " << (err_msg ? err_msg : "unknown")
               << " (sql=" << sql << ")";
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

statement::statement(database& db, std::string_view sql) {
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db.native_handle(), sql.data(),
                              static_cast<int>(sql.size()), &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare statement: "
               << sqlite3_errmsg(db.native_handle());
    return;
  }
  stmt_ = stmt;

  int count = sqlite3_bind_parameter_count(stmt_);
  for (int i = 1; i <= count; i++) {
    auto* name = sqlite3_bind_parameter_name(stmt_, i);
    if (name) {
      named_params_.emplace(name, i);
    } else {
      LOG(ERROR) << "[SQLITE3] Failed to get parameter name index(" << i << ")";
    }
  }
}

statement::statement(statement&& other) noexcept
    : stmt_{std::exchange(other.stmt_, nullptr)},
      named_params_{std::move(other.named_params_)} {}
statement& statement::operator=(statement&& other) noexcept {
  if (this != &other) {
    reset();
    stmt_ = std::exchange(other.stmt_, nullptr);
    named_params_ = std::move(other.named_params_);
  }
  return *this;
}

void statement::reset() {
  if (stmt_) {
    sqlite3_finalize(stmt_);
    stmt_ = nullptr;
  }
}
statement::~statement() { reset(); }

step_result statement::step() {
  int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_DONE) {
    return step_result::done;
  } else if (rc == SQLITE_ROW) {
    return step_result::row;
  } else {
    return step_result::error;
  }
}

bool statement::bind(int index, decltype(nullptr)) {
  return bind_check(sqlite3_bind_null(stmt_, index));
}
bool statement::bind(int index, int value) {
  return bind_check(sqlite3_bind_int(stmt_, index, value));
}
bool statement::bind(int index, sqlite3_int64 value) {
  return bind_check(sqlite3_bind_int64(stmt_, index, value));
}
bool statement::bind(int index, double value) {
  return bind_check(sqlite3_bind_double(stmt_, index, value));
}
bool statement::bind(int index, std::string_view value) {
  return bind_check(sqlite3_bind_text(stmt_, index, value.data(),
                                      static_cast<int>(value.size()),
                                      SQLITE_TRANSIENT));
}

bool statement::bind_check(int rc) {
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "sqlite3 bind failed: " << sqlite3_errstr(rc);
    return false;
  }
  return true;
}

int statement::get_named_param_index(std::string const& name) {
  if (name.empty()) {
    return 0;
  }
  if (name[0] == ':' || name[0] == '@' || name[0] == '$') {
    auto it = named_params_.find(name);
    if (it != named_params_.end()) {
      return it->second;
    }
    LOG(ERROR) << "No such parameter: " << name;
    return 0;
  } else {
    for (auto const& prefix : {":", "@", "$"}) {
      if (auto index = get_named_param_index(prefix + name); index != 0) {
        return index;
      }
    }
    LOG(ERROR) << "No such parameter: " << name;
    return 0;
  }
}

transaction::transaction(database& db) : db_(db) {
  db_.exec("BEGIN TRANSACTION");
}
void transaction::commit() {
  if (db_.exec("COMMIT")) {
    committed_ = true;
  }
}
transaction::~transaction() {
  if (!committed_) {
    db_.exec("ROLLBACK");
  }
}

}  // namespace ai::base
