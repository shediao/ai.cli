#pragma once

#include <sqlite3.h>

#include <optional>
#include <string>
#include <string_view>

namespace ai::base {
class database {
 public:
  explicit database(std::string_view path);
  ~database();
  database(database const&) = delete;
  database& operator=(database const&) = delete;
  database(database&& other) noexcept;
  database& operator=(database&& other) noexcept;

  inline sqlite3* native_handle() noexcept { return db_; }

  inline bool is_valid() const noexcept { return db_ != nullptr; }

  bool exec(std::string const& sql);

 private:
  void reset();
  sqlite3* db_{nullptr};
};

enum class step_result { row, done, error };
class statement {
 public:
  explicit statement(database& db, std::string_view sql);
  ~statement();
  statement(statement const&) = delete;
  statement& operator=(statement const&) = delete;
  statement(statement&& other) noexcept;
  statement& operator=(statement&& other) noexcept;

  inline bool is_valid() const noexcept { return stmt_ != nullptr; }

  inline sqlite3_stmt* native_handle() noexcept { return stmt_; }

  step_result step();
  bool bind(int index, decltype(nullptr));
  bool bind(int index, int value);
  bool bind(int index, sqlite3_int64 value);
  bool bind(int index, double value);
  bool bind(int index, std::string_view value);

  template <typename... Args>
  bool bind_all(Args&&... args) {
    int index = 1;
    return (bind(index++, std::forward<Args>(args)) && ...);
  }

  template <typename T>
  T get(int index);

 private:
  void reset();
  [[maybe_unused]] bool bind_check(int rc);
  sqlite3_stmt* stmt_{nullptr};
};

template <>
inline int statement::get<int>(int index) {
  return sqlite3_column_int(stmt_, index);
}

template <>
inline sqlite3_int64 statement::get<sqlite3_int64>(int index) {
  return sqlite3_column_int64(stmt_, index);
}

template <>
inline double statement::get<double>(int index) {
  return sqlite3_column_double(stmt_, index);
}

template <>
inline std::string statement::get<std::string>(int index) {
  auto const* text =
      reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
  if (!text) {
    return {};
  }
  return std::string{text,
                     static_cast<size_t>(sqlite3_column_bytes(stmt_, index))};
}

namespace {
template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

}  // namespace

template <typename T>
inline T statement::get(int index) {
  if constexpr (is_optional<T>::value) {
    if (sqlite3_column_type(stmt_, index) == SQLITE_NULL) {
      return std::nullopt;
    }
    return get<typename T::value_type>(index);
  } else {
    return get<T>(index);
  }
}

class transaction {
 public:
  explicit transaction(database& db);
  ~transaction();

  void commit();

 private:
  database& db_;
  bool committed_{false};
};

}  // namespace ai::base
