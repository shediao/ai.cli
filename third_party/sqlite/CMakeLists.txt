add_library(sqlite3 STATIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(
  sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_ENABLE_FTS5
                  SQLITE_OMIT_LOAD_EXTENSION)
if(MSVC)
  target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Create the canonical imported-target alias that find_package would provide
add_library(SQLite3::SQLite3 ALIAS sqlite3)
