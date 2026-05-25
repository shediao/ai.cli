This Content provides guidance to Ai commandline tool when working with code in this repository.

## Build & Test Commands

```bash
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
```

## Cross-Compilation

> **Note:** Cross-compilation environment is currently only available on macOS and Linux platforms.

When directories matching `build/{platform}-{arch}` exist (e.g., `build/darwin-arm64`, `build/linux-x86_64`, `build/mingw64-x86_64`, `build/windows-arm64`), the cross-compilation environment is already configured. Build directly with:

```bash
cmake --build build/{platform}-{arch}
```

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

Browse and manage GitHub Actions via the `gh` command:

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

**When the user wants to resolve GitHub Actions failures, they should first use `gh` commands to get logs and analyze the problem**, rather than blindly guessing the cause. How to get logs:

```bash
# Get logs of the latest run (usually the failed run)
gh run list --limit 1 --json databaseId -q '.[].databaseId' | xargs gh run view --log

# Get logs of a specific run-id (including failed steps)
gh run view <run-id> --log

# Get logs of the run corresponding to a specific commit
gh run list --commit <commit-sha> --limit 1 --json databaseId -q '.[].databaseId' | xargs gh run view --log

# Only view failed runs
gh run list --status failure --limit 5

# View logs of a failed job in a run (if the run has multiple jobs)
gh run view <run-id> --log --job <job-id>
```

**When fetching logs with `gh` commands, always prefer getting only the key information rather than the full logs unless absolutely necessary.** For example:

- Use `gh run view <run-id> --log --failed` to get only failed step logs (if supported).
- Pipe through `grep` to filter for error messages, failed test names, or compilation errors: `gh run view <run-id> --log 2>&1 | grep -E '(error:|FAILED|failure)'`.
- For test failures, look for the specific test output rather than the entire build log.
- Only fetch full logs when the filtered output does not provide enough context to diagnose the issue.

After getting the logs, identify the specific failure cause based on the error messages in the logs (compilation errors, test failures, environment issues, etc.), then make targeted fixes.

**Fix Verification**: If the failure occurs on a platform different from the current machine (e.g., currently on macOS ARM64, but the failure is on Linux x86_64), and a corresponding cross-compilation environment `build/{platform}-{arch}/` exists locally, after fixing, you **must** verify the build through that cross-compilation environment to ensure the fix also passes on the target platform:

```bash
# For example: fixed a compilation error on Linux x86_64, with build/linux-x86_64/ available locally
cmake --build build/linux-x86_64
```

If the corresponding cross-compilation environment does not exist locally, just push the fix directly and let CI verify.

## Architecture

This is a C++20 CLI chatbot that communicates with OpenAI-compatible APIs (DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama) with tool-calling capabilities.

### Entry & Dispatch

- `src/main.cpp` — `CurlGlobalInitGuard` RAII wrapper initializes/cleans CURL globally. On Windows, uses `wmain` + `SetConsoleOutputCP(CP_UTF8)`. Parses CLI args via `argparse::ArgParser` (defined in `src/args.cpp`, struct in `include/ai/args.h`), dispatches to `chat()`, `models()`, `history()`, or `update()`.
- `src/args.cpp` — defines all CLI subcommands, options, and flags. Provider aliases from `config.json` (e.g., `--deepseek`, `--openai`) set the `--base-url`. Automatically resolves API keys (`{ALIAS}_API_KEY` env var → config file), models (`{ALIAS}_API_MODEL` env var → config `default_model`), and proxy (`{ALIAS}_API_PROXY`). Default tools (`default`, `filesystem`, plus platform-specific shells) are auto-selected unless `--no-tools` or `--tools` is specified.

### Core Loop (`src/chat.cpp`)

The `chat()` function runs the conversation loop:

1. Builds a system prompt — auto-context with CWD, OS, architecture, shell, git branch from `build_default_system_prompt()` in `src/system_prompt.cpp`. If no user-supplied system prompt and the history is empty, the auto-context is used. If tools are enabled, appends `"Working Directory: <cwd>"`.
2. If `--continue-from <id>` or `-C` is given, loads previous messages from `HistoryDB::get_messages()`.
3. Calls `OpenAIClient::chat()` which sends the request and receives a `Response`.
4. If the response contains `tool_calls`, executes each tool via `call_tool()`, times and displays the result, appends results to `chat_history` as role=`"tool"` messages, and loops.
5. Breaks when `finish_reason` is `"stop"` (or on terminal error reasons: `"content_filter"`, `"length"`, `"insufficient_system_resources"`). Unknown finish reasons log a warning but continue.
6. On scope exit (via `base::scope_exit`), persists the conversation to SQLite (`HistoryDB::create_session()` with token statistics), generates a one-line topic via AI (`HistoryDB::generate_topic()`), and prints token usage summary.

Reasoning/thinking content (`reasoning_content` in response): in streaming mode, wrapped in ANSI-dim `<thinking>…</thinking>` blocks in the terminal; in non-streaming mode, prepended to content before printing.

### HTTP & Response Layer (`src/openai.cpp`, `src/response.cpp`, `include/ai/response.h`)

- `OpenAIClient` uses the **PIMPL idiom** — all CURL usage is hidden in `OpenAIClient::Impl`.
- `OpenAIClient::chat()`: builds the JSON request body (model, messages, stream options, tools converted to DeepSeek-compatible format, temperature/top_p/max_tokens/reasoning_effort/thinking params), sends via CURL.
- Two response paths:
  - **Non-streaming**: `Response::from_string()` → `Response::from_json()` parses a complete JSON response.
  - **Streaming (SSE)**: `StreamResponse::parse()` is set as the CURL write callback; parses `data: ` lines, `parse_line()` prints reasoning/content tokens to stdout with ANSI styling, accumulates JSON fragments. `StreamResponse::toResponse()` → `Response::from_sse_json()` reconstructs a unified `Response`.
- Image handling: local image files (by extension) are base64-encoded via `base64_encode()`; HTTP(S) URLs are downloaded to temp files via `base::download()`, then base64-encoded. Both are injected as `image_url` content blocks in the user message. Non-image URLs/paths are appended as text.

### Plugin-Style Tool System (`include/ai/function.h`, `src/function.cpp`)

Tools are registered via **static initialization** (which is why the build uses an OBJECT library — to prevent the linker from stripping static initializers).

Each tool category has:

1. A **JSON schema file** in `src/tools/` (e.g., `filesystem.json`, `bash.json`, `default.json`, `cmd.json`, `powershell.json`) defining the OpenAI function-calling schema.
2. A **`.h.in` template** (e.g., `filesystem_tools_json.h.in`) that `CMakeLists.txt` runs through `configure_file()` to embed the JSON as a `const std::string_view`.
3. A **registration function** (e.g., `regist_filesystem_tools()`) in the corresponding `.cpp` that calls `regist_tool_category()` (to register the schema) and `regist_tool_calls()` for each individual function.

Tool categories:

- **`default`** — session/environment queries: `get_working_directory`, `get_environment_variable`, `set_environment_variable`, `get_shell`, `get_operating_system`.
- **`filesystem`** — file operations: `read_file`, `read_multiple_files`, `write_file`, `edit_file`, `create_directory`, `list_directory`, `directory_tree`, `move_file`, `find_files`, `get_file_info`, `disk_space_info`, `execute_file`, `replace_lines`. Utilities shared across filesystem tools live in `src/tools/filesystem.cpp` (e.g., `expand_tilde`, `resolve_path`).
- **`bash`** — Unix/Linux/macOS shell; auto-detected on non-Windows or when `bash.exe` is in PATH.
- **`cmd`** — Windows Command Prompt (platform-conditional).
- **`powershell`** — Windows PowerShell (platform-conditional).

### Config (`src/config.cpp`, `include/ai/config.h`)

- Config file at OS-standard path (`ai::utils::app_data_dir("ai.cli") + "/config.json"`).
- `AppConfig` holds a `vector<ProviderConfig>`, each with `alias`, `base_url`, optional `api_key` and `default_model`.
- If the config file doesn't exist, `write_default_config_if_not_exists()` creates it with 6 pre-configured providers: deepseek, openai, gemini, qwen, moonshot, ollama.
- API keys resolved via: env var `{ALIAS}_API_KEY` → config file → empty.
- Model resolved via: CLI `--model` → env var `{ALIAS}_API_MODEL` → config file `default_model`.

### History (`src/history.cpp`, `include/ai/history.h`)

- SQLite-backed via `HistoryDB` using WAL journal mode and busy timeout (5s) for multi-process safety.
- Table `conversations_v1` with Unix-timestamp time fields; legacy `conversations` table kept for backward compatibility.
- Each conversation is a session with a unique session_id (format: `YYYYMMDD-HHMMSS-<16-hex>`), storing the full message JSON array, metadata (URL, model, working directory, parent session for continuations), token usage stats, and an AI-generated topic.
- `history` subcommand supports JSON array output (`--json`), line output (`--line`), human-readable text (`--text`), and single-session lookup (`--session`). Defaults to line format with newest-first ordering.

### Update (`src/update.cpp`, `include/ai/update.h`)

Self-update subcommand. Fetches the latest GitHub release from `api.github.com/repos/shediao/ai.cli/releases/latest`, compares semver with the current `GIT_VERSION`, downloads the platform-appropriate asset (`darwin-universal`, `linux-arm64`/`x64`, `windows-arm64`/`x64`, `mingw64-x64`, `freebsd-arm64`/`x64`), extracts the binary, and replaces the running executable. On Windows, a detached batch script handles the in-place replacement after process exit. Supports `--force` to skip version comparison.

### Models (`src/models.cpp`, `include/ai/models.h`)

`models` subcommand. Uses `OpenAIClient::models()` to GET the `/models` endpoint and prints model IDs.

### Base Utilities (`src/base/`)

| File                | Purpose                                 |
| ------------------- | --------------------------------------- |
| `file.h`/`.cc`      | Cross-platform file read/write          |
| `string.h`/`.cc`    | String utilities (split, UTF-8 helpers) |
| `terminal.h`/`.cc`  | TTY detection, interactive confirmation |
| `temp_file.h`/`.cc` | RAII temporary file (auto-deleted)      |
| `temp_dir.h`/`.cc`  | RAII temporary directory (auto-deleted) |
| `download.h`/`.cc`  | HTTP file download via CURL             |
| `scope_exit.h`      | Scope-guard cleanup (header-only)       |

### Key Dependencies (all via FetchContent)

| Library                                      | Purpose                                                     |
| -------------------------------------------- | ----------------------------------------------------------- |
| `argparse.hpp` (custom fork)                 | CLI argument parsing                                        |
| `nlohmann/json` (vendored in `third_party/`) | JSON handling                                               |
| `libcurl` (fetched or system)                | HTTP client (minimal build: HTTP-only, platform-native SSL) |
| `sqlite3` (amalgamation v3.53.0)             | Chat history persistence                                    |
| `subprocess.hpp` (custom fork)               | Subprocess execution for tools                              |
| `environment.hpp` (custom fork)              | Cross-platform env var access                               |
| `base64.hpp` (custom fork)                   | Base64 encoding for image input                             |
| `utfx.hpp` (custom fork)                     | UTF-8 validation                                            |
| Google Test (fetched)                        | Unit testing                                                |

All custom forks are under `github.com/shediao/*`.

### Code Style

- Google style, 2-space indent, C++20.
- `.clang-tidy` disables: `llvm-header-guard`, `modernize-use-trailing-return-type`, `readability-identifier-naming`, `cppcoreguidelines-pro-type-cstyle-cast`, and several modernize checks.
- Use `ai::term::` namespace for ANSI terminal colors/styles instead of raw escape codes.
- Use `ai::base::scope_exit` for scope-guard cleanup instead of manual try/finally patterns.
- Use the `LOG(LEVEL)` macro from `ai/logging.h` (levels: DEBUG, INFO, WARNING, ERROR, FATAL).
- Include order: standard library → third-party → project headers.
- All source code files must use LF (`\n`) line endings. Do not introduce CRLF (`\r\n`) when editing files.

# Ai Coding Guidelines

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:

- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:

- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:

- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:

- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
