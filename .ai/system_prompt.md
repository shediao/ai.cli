This Content provides guidance to Ai tool when working with code in this repository.

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
