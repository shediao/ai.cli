cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  cmake_policy(SET CMP0135 NEW)
endif()

# ── Project ─────────────────────────────────────────────────────────────────
project(
  ai_cli
  VERSION 0.1.0
  LANGUAGES CXX C)

# ── Default build type ─────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE
        STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel..."
        FORCE)
endif()

# ── C++ standard ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

if(NOT MSYS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ── Symbol visibility ──────────────────────────────────────────────────────
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ── Compiler warnings ──────────────────────────────────────────────────────
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /utf-8 /EHsc")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# ── Sanitizers (Linux/macOS/Windows) ──────────────────────────────────────
option(AICLI_ENABLE_ASAN "Enable Address/Undefined Sanitizers" ON)

if(AICLI_ENABLE_ASAN AND NOT CMAKE_CROSSCOMPILING)
  if(MSVC)
    # MSVC AddressSanitizer (VS 2019 16.4+), only for Debug/RelWithDebInfo
    add_compile_options(
      "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/fsanitize=address>")
    # ASAN on Windows requires dynamically linked CRT (/MD or /MDd)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux|Darwin")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      add_compile_options(
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-fsanitize=address,undefined>"
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-fno-omit-frame-pointer>"
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-fno-optimize-sibling-calls>"
      )
      add_link_options(
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-fsanitize=address,undefined>"
      )
    endif()
  endif()
endif()

# ── Testing ────────────────────────────────────────────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(AICLI_BUILD_TESTS "Build unit tests" ON)
else()
  option(AICLI_BUILD_TESTS "Build unit tests" OFF)
endif()

# ── Options ────────────────────────────────────────────────────────────────
option(AICLI_USE_SYSTEM_CURL "Use system-installed libcurl" OFF)

# ── Git version ────────────────────────────────────────────────────────────
execute_process(
  COMMAND git describe --tags --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_VERSION)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.VERSION")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.VERSION" GIT_VERSION)
    string(STRIP "${GIT_VERSION}" GIT_VERSION)
  endif()
endif()
if(NOT GIT_VERSION)
  set(GIT_VERSION "unknown")
endif()
message(STATUS "Git version: ${GIT_VERSION}")

# ── Dependencies ───────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

add_subdirectory(third_party/sqlite)

# ── Object library (all sources except main) ───────────────────────────────
# Use OBJECT library instead of STATIC to ensure all static initializers (e.g.
# regist_tool_category) are linked into the final executable.
add_library(
  ai_core OBJECT
  src/chat.cc
  src/system_prompt.cc
  src/args.cc
  src/config.cc
  src/models.cc
  src/openai.cc
  src/response.cc
  src/base/base64.cc
  src/base/database.cc
  src/base/download.cc
  src/base/file.cc
  src/base/glob.cc
  src/base/io.cc
  src/base/matchglob.cc
  src/base/logging.cc
  src/base/string.cc
  src/base/temp_dir.cc
  src/base/temp_file.cc
  src/base/terminal.cc
  src/tools/execute.cc
  src/tools/filesystem.cc
  src/tools/filesystem/read_file.cc
  src/tools/filesystem/read_multiple_files.cc
  src/tools/filesystem/write_file.cc
  src/tools/filesystem/edit_file.cc
  src/tools/filesystem/create_directory.cc
  src/tools/filesystem/list_directory.cc
  src/tools/filesystem/directory_tree.cc
  src/tools/filesystem/move_file.cc
  src/tools/filesystem/find_files.cc
  src/tools/filesystem/get_file_info.cc
  src/tools/filesystem/disk_space_info.cc
  src/tools/execute/bash.cc
  src/tools/execute/cmd.cc
  src/tools/execute/powershell.cc
  src/tools/execute/execute_command.cc
  src/tools/filesystem/replace_lines.cc
  src/tools/default.cc
  src/tools/interactive/ask_user.cc
  src/function.cc
  src/utils.cc
  src/history.cc
  src/update.cc)

# ── Include directories ────────────────────────────────────────────────────
target_include_directories(
  ai_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
         ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(ai_core PUBLIC GIT_VERSION="${GIT_VERSION}")

# ── Platform-specific settings ─────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(ai_core PUBLIC UNICODE _UNICODE NOMINMAX)
  target_link_libraries(ai_core PUBLIC ole32)
endif()

if(MINGW)
  target_link_options(ai_core PRIVATE -static-libgcc)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(ai_core PRIVATE -static-libgcc -static-libstdc++)
endif()

# ── Link dependencies ──────────────────────────────────────────────────────
target_link_libraries(
  ai_core
  PUBLIC argparse::argparse
         base64::base64
         subprocess::subprocess
         environment::environment
         utfx::utfx
         CURL::libcurl
         SQLite3::SQLite3)

# ── Executable ─────────────────────────────────────────────────────────────
add_executable(ai src/main.cc $<TARGET_OBJECTS:ai_core>)

set_target_properties(ai PROPERTIES OUTPUT_NAME "ai")

target_link_libraries(ai PRIVATE ai_core)

if(MINGW)
  set_target_properties(ai PROPERTIES LINK_FLAGS "-mconsole -municode")
endif()

# ── Tests ──────────────────────────────────────────────────────────────────
if(AICLI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ────────────────────────────────────────────────────────────────
install(TARGETS ai RUNTIME DESTINATION bin)
