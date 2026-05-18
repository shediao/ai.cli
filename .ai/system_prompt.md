# Ai.md

This file provides guidance to Ai Coder when working with code in this repository.

## Build & Test Commands

````bash
# Build (creates build/Debug/ai or build/Release/ai)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build and run all tests
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build

# Run a single test
ctest --test-dir build -R test_utils

# Build options
cmake -B build -S . -DAICLI_BUILD_TESTS=OFF       # skip tests
cmake -B build -S . -DAICLI_USE_SYSTEM_CURL=ON    # use system libcurl
cmake -B build -S . -DAICLI_ENABLE_ASAN=OFF       # disable sanitizers

# Format code (Google style, 2-space indent, C++20)
clang-format -i src/*.cpp include/ai/*.h tests/*.cc tests/*.cpp
cmake-format -i CMakeLists.txt tests/CMakeLists.txt
## Cross-Compilation

> **Note:** Cross-compilation environment is currently only available on macOS and Linux platforms.

When directories matching `build/{platform}-{arch}` exist (e.g., `build/darwin-arm64`, `build/linux-x86_64`, `build/mingw64-x86_64`, `build/windows-arm64`), the cross-compilation environment is already configured. Build directly with:

```bash
cmake --build build/{platform}-{arch}
````

Supported build directories:

| Directory              | Target                 |
| ---------------------- | ---------------------- |
| `build/darwin-arm64`   | macOS ARM64            |
| `build/darwin-x86_64`  | macOS x86_64           |
| `build/linux-arm64`    | Linux ARM64            |
| `build/linux-x86_64`   | Linux x86_64           |
| `build/mingw64-x86_64` | Windows x86_64 (MinGW) |
| `build/windows-arm64`  | Windows ARM64 (MSVC)   |
| `build/windows-x86_64` | Windows x86_64 (MSVC)  |

When source files are added/removed or CMake options change (making it necessary to re-run CMake configuration), simply `touch CMakeLists.txt` and the next `cmake --build` will automatically re-generate the build system:

```bash
touch CMakeLists.txt
cmake --build build/{platform}-{arch}
```

MSVC note: `test_stream_response.cc` and `test_tool_calls_stream_response.cc` are excluded from MSVC non-Clang builds due to a very long string literal that truncates the `cl.exe` command line.

## CI

If the remote repository URL is `http://github.com/*` or `git@github.com:*`, then GitHub Actions (configuration files located at `.github/workflows/*.yml`) is used as CI.

通过 `gh` 命令浏览和操作 GitHub Actions：

```bash
# View recent workflow runs
gh run list

# View details of a specific run
gh run view <run-id>

# View logs of a specific run
gh run view <run-id> --log

# Manually trigger a workflow
gh workflow run <workflow-name>

# List all workflows
gh workflow list

# View workflow file contents
gh workflow view <workflow-name>
```

## Architecture

This is a C++20 CLI chatbot that communicates with OpenAI-compatible APIs (DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama) with tool-calling capabilities.

### Entry & Dispatch

- `src/main.cpp` — initializes CURL globally, parses CLI args via `argparse::ArgParser`, dispatches to `chat()`, `models()`, `history()`, or `update()`.

### Core Loop (`src/chat.cpp`)

The `chat()` function runs the conversation loop:

1. Builds a system prompt (auto-context with CWD, OS, shell, git info from `system_prompt.cpp`).
2. Calls `OpenAIClient::chat()` which sends the request and receives a `Response`.
3. If the response contains `tool_calls`, executes each tool via `call_tool()`, appends results to `chat_history` as role=`"tool"` messages, and loops.
4. Breaks when finish_reason is `"stop"` (or on error reasons like `"content_filter"`, `"length"`).
5. On exit, persists the conversation to SQLite and generates a one-line topic via AI.

### HTTP & Response Layer (`src/openai.cpp`, `include/ai/response.h`)

- `OpenAIClient` uses the **PIMPL idiom** — all CURL usage is hidden in `OpenAIClient::Impl`.
- Two response paths:
  - **Non-streaming**: `Response::from_string()` parses a complete JSON response.
  - **Streaming (SSE)**: `StreamResponse::parse()` is set as the CURL write callback; tokens are printed to stdout as they arrive. `StreamResponse::toResponse()` reconstructs a `Response` from accumulated SSE fragments.
- Image handling: local image files are base64-encoded; URLs are downloaded to temp files, then base64-encoded. Both are injected as `image_url` content blocks in the user message.

### Plugin-Style Tool System (`include/ai/function.h`, `src/function.cpp`)

Tools are registered via **static initialization** (which is why the build uses an OBJECT library — to prevent the linker from stripping static initializers).

Each tool category has:

1. A **JSON schema file** in `src/tools/` (e.g., `filesystem.json`, `bash.json`, `default.json`) defining the OpenAI function-calling schema.
2. A **`.h.in` template** (e.g., `filesystem_tools_json.h.in`) that `CMakeLists.txt` runs through `configure_file()` to embed the JSON as a `const std::string_view`.
3. A **registration function** (e.g., `regist_filesystem_tools()`) in the corresponding `.cpp` that calls `regist_tool_category()` (to register the schema) and `regist_tool_calls()` for each individual function.

Tool categories: `bash`, `filesystem`, `git` (embedded in filesystem), `default`. `bash`, `cmd`, and `powershell` are platform-specific shell execution.

### Config (`src/config.cpp`)

- Config file at OS-standard path (`ai::utils::app_data_dir("ai.cli") + "/config.json"`).
- `AppConfig` holds a `vector<ProviderConfig>`, each with `alias`, `base_url`, optional `api_key` and `default_model`.
- API keys resolved via: env var `{ALIAS}_API_KEY` → config file → empty.
- Model resolved via: CLI `--model` → env var `{ALIAS}_API_MODEL` → config file `default_model`.

### History (`src/history.cpp`)

- SQLite-backed via `HistoryDB` using WAL journal mode for multi-process safety.
- Each conversation is a session with a UUID, storing the full message JSON array, metadata (URL, model, working directory, parent session for continuations), and an AI-generated topic.

### Key Dependencies (all via FetchContent)

| Library                                      | Purpose                         |
| -------------------------------------------- | ------------------------------- |
| `argparse.hpp` (custom fork)                 | CLI argument parsing            |
| `nlohmann/json` (vendored in `third_party/`) | JSON handling                   |
| `libcurl` (fetched or system)                | HTTP client                     |
| `sqlite3` (amalgamation)                     | Chat history persistence        |
| `subprocess.hpp` (custom fork)               | Subprocess execution for tools  |
| `environment.hpp` (custom fork)              | Cross-platform env var access   |
| `base64.hpp` (custom fork)                   | Base64 encoding for image input |
| `utfx.hpp` (custom fork)                     | UTF-8 validation                |
| Google Test (fetched)                        | Unit testing                    |

All custom forks are under `github.com/shediao/*`.

### Code Style

- Google style, 2-space indent, C++20.
- `.clang-tidy` disables: `llvm-header-guard`, `modernize-use-trailing-return-type`, `readability-identifier-naming`, `cppcoreguidelines-pro-type-cstyle-cast`, and several modernize checks.
- Use `ai::term::` namespace for ANSI terminal colors/styles instead of raw escape codes.
- Use `ai::utils::AutoRun` for scope-guard cleanup instead of manual try/finally patterns.
- Use the `LOG(LEVEL)` macro from `ai/logging.h` (levels: DEBUG, INFO, WARNING, ERROR, FATAL).
- Include order: standard library → third-party → project headers.
