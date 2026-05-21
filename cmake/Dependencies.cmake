# ==============================================================================
# Dependencies - FetchContent declarations for all third-party libraries
# ==============================================================================

include(FetchContent)

# ── argparse (command-line argument parsing) ────────────────────────────────
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/shediao/argparse.hpp
  GIT_TAG v0.1.2)
FetchContent_MakeAvailable(argparse)

# ── base64 (base64 encoding/decoding) ──────────────────────────────────────
FetchContent_Declare(
  base64
  GIT_REPOSITORY https://github.com/shediao/base64.hpp
  GIT_TAG 6ff32c627e55d5360feb08476369a83a3c46c376)
FetchContent_MakeAvailable(base64)

# ── subprocess (subprocess execution) ──────────────────────────────────────
FetchContent_Declare(
  subprocess
  GIT_REPOSITORY https://github.com/shediao/subprocess.hpp
  GIT_TAG v0.0.18)
FetchContent_MakeAvailable(subprocess)

# ── environment (environment variable access) ──────────────────────────────
FetchContent_Declare(
  environment
  GIT_REPOSITORY https://github.com/shediao/environment.hpp
  GIT_TAG v0.0.6)
FetchContent_MakeAvailable(environment)

# ── utfx (UTF-8 validation) ────────────────────────────────────────────────
FetchContent_Declare(
  utfx
  GIT_REPOSITORY https://github.com/shediao/utfx.hpp
  GIT_TAG 1325b349d1d044a1b728daa56ecc45568234486d)
FetchContent_MakeAvailable(utfx)

# ── libcurl (HTTP client) ──────────────────────────────────────────────────
if(AICLI_USE_SYSTEM_CURL)
  find_package(CURL QUIET)
endif()

if(AICLI_USE_SYSTEM_CURL AND CURL_FOUND)
  message(STATUS "Found CURL version: ${CURL_VERSION_STRING}")
  message(STATUS "Using CURL include dir(s): ${CURL_INCLUDE_DIRS}")
  message(STATUS "Using CURL lib(s): ${CURL_LIBRARIES}")
else()
  message(STATUS "Fetching CURL...")
  FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_9_1
    UPDATE_DISCONNECTED ON)

  # Configure curl build options for minimal footprint
  set(BUILD_CURL_EXE
      OFF
      CACHE BOOL "Don't build curl executable")
  set(HTTP_ONLY
      ON
      CACHE BOOL "Only enable HTTP protocol")
  set(CURL_WERROR
      OFF
      CACHE BOOL "Turn compiler warnings into errors")
  set(BUILD_SHARED_LIBS
      OFF
      CACHE BOOL "Build shared libraries")
  set(BUILD_STATIC_LIBS
      ON
      CACHE BOOL "Build static libraries")
  set(ENABLE_ARES
      OFF
      CACHE BOOL "Enable c-ares support")

  # Disable unused protocols and features
  foreach(
    _proto IN
    ITEMS ALTSVC
          SRP
          COOKIES
          AWS
          DICT
          DOH
          FILE
          FTP
          GETOPTIONS
          GOPHER
          HSTS
          IMAP
          LDAP
          LDAPS
          MQTT
          BINDLOCAL
          NETRC
          NTLM
          POP3
          PROGRESS_METER
          IPFS
          RTSP
          SMB
          SMTP
          WEBSOCKETS
          TELNET
          TFTP
          VERBOSE_STRINGS)
    set(CURL_DISABLE_${_proto}
        ON
        CACHE BOOL "Disable ${_proto}")
  endforeach()

  set(BUILD_LIBCURL_DOCS
      OFF
      CACHE BOOL "Build libcurl man pages")
  set(BUILD_MISC_DOCS
      OFF
      CACHE BOOL "Build misc man pages")
  set(ENABLE_CURL_MANUAL
      OFF
      CACHE BOOL "Build the man page for curl")
  set(CURL_USE_LIBSSH2
      OFF
      CACHE BOOL "Use libssh2")

  # Platform-specific SSL backends
  if(APPLE)
    set(CURL_USE_SECTRANSP
        ON
        CACHE BOOL "Enable Apple OS native SSL/TLS")
    set(CURL_USE_OPENSSL
        OFF
        CACHE BOOL "Enable OpenSSL for SSL/TLS")
  endif()
  if(WIN32)
    set(CURL_USE_SCHANNEL
        ON
        CACHE BOOL "Enable Windows native SSL/TLS (Schannel)")
    set(CURL_USE_OPENSSL
        OFF
        CACHE BOOL "Enable OpenSSL for SSL/TLS")
  endif()
  set(CURL_USE_SSL
      ON
      CACHE BOOL "Enable SSL/TLS")

  set(USE_LIBIDN2
      OFF
      CACHE BOOL "Use libidn2 for IDN support")
  set(USE_APPLE_IDN
      OFF
      CACHE BOOL "Use Apple built-in IDN support")
  set(USE_WIN32_IDN
      OFF
      CACHE BOOL "Use WinIDN for IDN support")

  FetchContent_MakeAvailable(curl)
endif()

# ── SQLite3 (chat history database) ────────────────────────────────────────
set(SQLITE_ARCHIVE "sqlite-amalgamation-3530000.zip")
set(SQLITE_URL "https://sqlite.org/2026/${SQLITE_ARCHIVE}")

FetchContent_Declare(sqlite3_amalgamation URL ${SQLITE_URL}
                                              DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_GetProperties(sqlite3_amalgamation)
if(NOT sqlite3_amalgamation_POPULATED)
  FetchContent_MakeAvailable(sqlite3_amalgamation)

  add_library(sqlite3 STATIC "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c")
  target_include_directories(sqlite3
                             PUBLIC "${sqlite3_amalgamation_SOURCE_DIR}")
  target_compile_definitions(
    sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_ENABLE_FTS5
                    SQLITE_OMIT_LOAD_EXTENSION)
  if(MSVC)
    target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  # Create the canonical imported-target alias that find_package would provide
  add_library(SQLite3::SQLite3 ALIAS sqlite3)
endif()
