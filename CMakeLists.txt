cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  cmake_policy(SET CMP0135 NEW)
endif()

project(
  ai_cli
  VERSION 0.1.0
  LANGUAGES CXX)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE
        STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel..."
        FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT MSYS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /utf-8 /EHsc")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

if(NOT CMAKE_CROSSCOMPILING AND CMAKE_HOST_SYSTEM_NAME MATCHES "Linux|Darwin")
  option(AICLI_ENABLE_ASASN "Set to ON to build tests" ON)
else()
  option(AICLI_ENABLE_ASASN "Set to ON to build tests" OFF)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(AICLI_BUILD_TESTS "Set to ON to build tests" ON)
else()
  option(AICLI_BUILD_TESTS "Set to OFF to build tests" OFF)
endif()

if(WIN32)
  option(AICLI_USE_SYSTEM_CURL "Use system curl" OFF)
else()
  option(AICLI_USE_SYSTEM_CURL "Use system curl" OFF)
endif()

if(AICLI_ENABLE_ASASN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls"
    )
  endif()
endif()

# 添加FetchContent支持
include(FetchContent)

# 添加argparse用于命令行参数解析
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/shediao/argparse.hpp
  GIT_TAG 8056522918fdd7f0c5fe632a095304f19893cfc4)
FetchContent_MakeAvailable(argparse)

# 添加base64用于嵌入文件
FetchContent_Declare(
  base64
  GIT_REPOSITORY https://github.com/shediao/base64.hpp
  GIT_TAG 6ff32c627e55d5360feb08476369a83a3c46c376)
FetchContent_MakeAvailable(base64)

FetchContent_Declare(
  subprocess
  GIT_REPOSITORY https://github.com/shediao/subprocess.hpp
  GIT_TAG v0.0.5)
FetchContent_MakeAvailable(subprocess)

FetchContent_Declare(
  environment
  GIT_REPOSITORY https://github.com/shediao/environment.hpp
  GIT_TAG v0.0.4)
FetchContent_MakeAvailable(environment)

FetchContent_Declare(
  utfx
  GIT_REPOSITORY https://github.com/shediao/utfx.hpp
  GIT_TAG 272b036d6a25e3a24cf1685d5ce361003e2cc576)
FetchContent_MakeAvailable(utfx)

# 添加libcurl用于HTTP请求

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

  # 设置 curl 的构建选项
  set(BUILD_CURL_EXE
      OFF
      CACHE BOOL "Don't build curl executable")
  set(HTTP_ONLY
      ON
      CACHE BOOL "Only enable HTTP protocol")

  set(CURL_WERROR
      OFF
      CACHE BOOL "Turn compiler warnings into errors")
  set(BUILD_CURL_EXE
      ON
      CACHE BOOL "Build curl executable")
  set(BUILD_SHARED_LIBS
      OFF
      CACHE BOOL "Build shared libraries")
  set(BUILD_STATIC_LIBS
      ON
      CACHE BOOL "Build static libraries")
  set(ENABLE_ARES
      OFF
      CACHE BOOL "Enable c-ares support")
  set(CURL_DISABLE_ALTSVC
      ON
      CACHE BOOL "Disable alt-svc support")
  set(CURL_DISABLE_SRP
      ON
      CACHE BOOL "Disable TLS-SRP support")
  set(CURL_DISABLE_COOKIES
      ON
      CACHE BOOL "Disable cookies support")
  set(CURL_DISABLE_AWS
      ON
      CACHE BOOL "Disable aws-sigv4")
  set(CURL_DISABLE_DICT
      ON
      CACHE BOOL "Disable DICT")
  set(CURL_DISABLE_DOH
      ON
      CACHE BOOL "Disable DNS-over-HTTPS")
  set(CURL_DISABLE_FILE
      ON
      CACHE BOOL "Disable FILE")
  set(CURL_DISABLE_FTP
      ON
      CACHE BOOL "Disable FTP")
  set(CURL_DISABLE_GETOPTIONS
      ON
      CACHE
        BOOL
        "Disable curl_easy_options API for existing options to curl_easy_setopt"
  )
  set(CURL_DISABLE_GOPHER
      ON
      CACHE BOOL "Disable Gopher")
  set(CURL_DISABLE_HSTS
      ON
      CACHE BOOL "Disable HSTS support")
  set(CURL_DISABLE_IMAP
      ON
      CACHE BOOL "Disable IMAP")
  set(CURL_DISABLE_LDAP
      ON
      CACHE BOOL "Disable LDAP")
  set(CURL_DISABLE_LDAPS
      ON
      CACHE BOOL "Disable LDAPS")
  set(CURL_DISABLE_MQTT
      ON
      CACHE BOOL "Disable MQTT")
  set(CURL_DISABLE_BINDLOCAL
      ON
      CACHE BOOL "Disable local binding support")
  set(CURL_DISABLE_NETRC
      ON
      CACHE BOOL "Disable netrc parser")
  set(CURL_DISABLE_NTLM
      ON
      CACHE BOOL "Disable NTLM support")
  set(CURL_DISABLE_POP3
      ON
      CACHE BOOL "Disable POP3")
  set(CURL_DISABLE_PROGRESS_METER
      ON
      CACHE BOOL "Disable built-in progress meter")
  set(CURL_DISABLE_IPFS
      ON
      CACHE BOOL "Disable IPFS")
  set(CURL_DISABLE_RTSP
      ON
      CACHE BOOL "Disable RTSP")
  set(CURL_DISABLE_SMB
      ON
      CACHE BOOL "Disable SMB")
  set(CURL_DISABLE_SMTP
      ON
      CACHE BOOL "Disable SMTP")
  set(CURL_DISABLE_WEBSOCKETS
      ON
      CACHE BOOL "Disable WebSocket")
  set(CURL_DISABLE_TELNET
      ON
      CACHE BOOL "Disable Telnet")
  set(CURL_DISABLE_TFTP
      ON
      CACHE BOOL "Disable TFTP")
  set(CURL_DISABLE_VERBOSE_STRINGS
      ON
      CACHE BOOL "Disable verbose strings")
  set(BUILD_LIBCURL_DOCS
      OFF
      CACHE BOOL "Build libcurl man pages")
  set(BUILD_MISC_DOCS
      OFF
      CACHE BOOL "Build misc man pages (e.g. curl-config and mk-ca-bundle)")
  set(ENABLE_CURL_MANUAL
      OFF
      CACHE BOOL
            "Build the man page for curl and enable its -M/--manual option")

  set(CURL_USE_LIBSSH2
      OFF
      CACHE BOOL "Use libssh2")

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
      CACHE BOOL "Enable Windows native SSL/TLS (Schannel)")

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

# 添加源文件
add_executable(
  ai
  src/main.cpp
  src/chat.cpp
  src/args.cpp
  src/logging.cpp
  src/models.cpp
  src/openai.cpp
  src/response.cpp
  src/tools/filesystem.cpp
  src/tool_calls.cpp
  src/utils.cpp
  src/base64.cpp)

if(APPLE)
  target_sources(ai PRIVATE src/clip_other.cpp)
elseif(WIN32)
  target_sources(ai PRIVATE src/clip_win.cpp)
elseif(LINUX)
  target_sources(ai PRIVATE src/clip_linux.cpp)
else()
  target_sources(ai PRIVATE src/clip_other.cpp)
endif()

set_target_properties(ai PROPERTIES OUTPUT_NAME "ai")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(ai PRIVATE UNICODE _UNICODE)
endif()

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/filesystem.json
     FILESYSTEM_TOOLS_JSON_DATA)
configure_file(src/tools/filesystem_tools_json.h.in filesystem_tools_json.h)

# 添加头文件路径
target_include_directories(
  ai
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
          ${CMAKE_CURRENT_BINARY_DIR})

# 链接依赖库
target_link_libraries(
  ai PRIVATE argparse::argparse base64::base64 subprocess::subprocess
             environment::environment utfx::utfx CURL::libcurl)

if(WIN32)
  target_link_libraries(ai PRIVATE "-luser32")
endif()

if(MINGW)
  target_link_options(ai PRIVATE -static-libgcc)
  set_target_properties(ai PROPERTIES LINK_FLAGS "-mconsole -municode")
endif()

if(LINUX)
  target_link_options(ai PRIVATE -static-libgcc -static-libstdc++)
endif()

if(AICLI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# 安装目标
install(TARGETS ai RUNTIME DESTINATION bin)
