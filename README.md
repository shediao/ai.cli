# ai

[![CMAKE](https://github.com/shediao/ai.cli/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/shediao/ai.cli/actions/workflows/cmake-multi-platform.yml)
[![MSYS2](https://github.com/shediao/ai.cli/actions/workflows/msys2.yml/badge.svg)](https://github.com/shediao/ai.cli/actions/workflows/msys2.yml)
[![Release](https://github.com/shediao/ai.cli/actions/workflows/release.yml/badge.svg)](https://github.com/shediao/ai.cli/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A powerful command-line AI chatbot with multi-provider support and tool-calling capabilities. Uses OpenAI-compatible APIs to interact with DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama, and more.

## Quick Start

```bash
@ds() { ai chat --stream --deepseek "$@"; }

@ds "review my staged code changes"

@ds "solve the latest GitHub Action failure issue"

```

## Features

- **Multi-provider support** — DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama, and any OpenAI-compatible API via config
- **Tool calling** — AI can execute shell commands (bash, cmd, powershell), read/write/edit files, execute binaries, ask the user for input, and more (see [Tools](#tools))
- **Streaming output** — tokens are displayed as they are generated
- **Image input** — attach images via local files or URLs for vision-capable models
- **Chat history** — conversation is persisted to disk; continue previous sessions with `-C` or `--continue-from`
- **History browser** — `ai history` lists, searches, and inspects past conversations
- **Self-update** — `ai update` downloads the latest release from GitHub and replaces the current binary
- **Editor input** — if no prompt is given and stdin is a TTY, opens `$EDITOR` for composing the prompt
- **Pipe / stdin** — read prompts from stdin; use `-` as a positional to read from the pipe
- **Configurable system prompt** — automatic context injection (CWD, git branch, OS, shell)
- **Shell completions** — built-in generation for bash, zsh, and fish
- **Adjustable parameters** — temperature, top-p, max tokens, reasoning effort, thinking toggle
- **Config file** — manage providers, default models, and API keys in `config.json`
- **Logging** — configurable log levels; direct output to a file via `--log-to` (defaults to stderr)

## Build Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20 or higher
- libcurl (or let CMake fetch it automatically)

## Build Steps

```bash
# Quick build (both Debug and Release)
./build.sh

# Or manually:
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is `build/Release/ai` (or `build/Debug/ai`).

### Build Options

| Option                  | Default | Description                                         |
| ----------------------- | ------- | --------------------------------------------------- |
| `AICLI_BUILD_TESTS`     | ON      | Build unit tests                                    |
| `AICLI_ENABLE_ASAN`     | ON      | Enable Address/Undefined Sanitizers in Debug        |
| `AICLI_USE_SYSTEM_CURL` | OFF     | Use system-installed libcurl instead of fetching it |

## Usage

### Basic Chat

```bash
# Show help
ai --help
ai chat --help

# Simple prompt
ai chat "Hello, World!"

# Use a specific model
ai chat --model gpt-4o "Write a quicksort algorithm"

# Use a provider alias (sets the base URL and default model)
ai chat --deepseek "Explain quantum computing"
ai chat --openai   "Write a haiku"
ai chat --gemini   "Summarize this article"
ai chat --qwen     "Translate to Chinese"
ai chat --moonshot "Code review this"
ai chat --ollama   "Hello local model"

# Enable streaming
ai chat --stream "Tell me a story"

# Streaming with token usage
ai chat --stream --stream-include-usage "What is 2+2?"

# Custom system prompt
ai chat --system-prompt "You are a professional C++ programmer" "Write code"

# Set model parameters
ai chat --temperature 0.7 --top-p 0.9 --max-tokens 4096 "Write a poem"

# Reasoning effort (for models that support it)
ai chat --reasoning-effort high "Solve this complex math problem"

# Disable thinking (for models that support it)
ai chat --no-thinking "Quick question"
```

### Image Input

```bash
# Local image file
ai chat --model gpt-4o "Describe this image" photo.png

# Image from URL
ai chat --model gpt-4o "What's in this picture?" https://example.com/photo.jpg

# Multiple images and text
ai chat --model gpt-4o "Compare these" img1.jpg img2.jpg
```

### Pipe / Stdin Input

```bash
# Pipe content directly
echo "Explain this code:" | cat - src/main.cpp | ai chat --stream

# Use '-' as a placeholder for stdin
git diff | ai chat --stream "Review this diff: -"

# Read from a file
ai chat --stream "Summarize:" < README.md
```

### Editor Input

When running interactively without a prompt, `ai` opens your `$EDITOR`:

```bash
ai chat --stream
# Opens $EDITOR (vim/nano/etc.) — write your prompt, save & exit
```

### Continue a Previous Conversation

```bash
# Continue from the last saved chat history
ai chat --stream -C "What else can you tell me?"

# Continue from a specific session (find session IDs with `ai history`)
ai chat --stream --continue-from 20250101-120000-a1b2c3d4e5f6g7h8 "Continue"
```

### List Available Models

```bash
# List models from the default provider (first in config.json)
ai models

# List models from a specific provider
ai models --openai
ai models --gemini
```

### Browse Chat History

```bash
# List recent sessions (default: 20)
ai history

# List all sessions
ai history --limit 0

# View a specific session in detail
ai history --session 20250101-120000-a1b2c3d4e5f6g7h8

# Output as JSON with selected fields
ai history --json session-id,topic,start

# Output in human-readable text format
ai history --text

# Custom pipe-delimited line format
ai history --line session_id,start,work_dir,topic
```

### Self-Update

```bash
# Check for a newer version on GitHub and update
ai update

# Force update even if already on latest
ai update --force
```

### List / Disable / Configure Tools

```bash
# List all available tool categories and their functions
ai chat --list-tools

# Disable all tools
ai chat --no-tools "Hello"

# Enable only specific tool categories
ai chat --tools execute "List files in /tmp"
ai chat --tools filesystem,default "What is the current directory?"

# Force a tool call
ai chat --tool-choice required --tools execute "List the current directory"
```

## Configuration

### Config File

`ai` creates a config file at:

- **Linux**: `~/.local/share/ai.cli/config.json`
- **macOS**: `~/Library/Application Support/ai.cli/config.json`
- **Windows**: `%USERPROFILE%\AppData\Local\Ai\ai.cli\config.json`

The default config:

```json
{
  "providers": [
    {
      "alias": "deepseek",
      "base_url": "https://api.deepseek.com",
      "default_model": "deepseek-v4-pro"
    },
    {
      "alias": "openai",
      "base_url": "https://api.openai.com/v1",
      "default_model": "gpt-4o"
    },
    {
      "alias": "gemini",
      "base_url": "https://generativelanguage.googleapis.com/v1beta/openai/",
      "default_model": "gemini-flash-latest"
    },
    {
      "alias": "qwen",
      "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1/",
      "default_model": "qwen-max-latest"
    },
    {
      "alias": "moonshot",
      "base_url": "https://api.moonshot.cn/v1",
      "default_model": "kimi-k2.6"
    },
    { "alias": "ollama", "base_url": "http://127.0.0.1:11434/v1" }
  ]
}
```

You can set `api_key` and `default_model` per provider in this file, or use environment variables.

### Environment Variables

Environment variables use the provider alias as a prefix (uppercased):

```bash
# API keys
export DEEPSEEK_API_KEY="your-deepseek-api-key"
export OPENAI_API_KEY="your-openai-api-key"
export GEMINI_API_KEY="your-google-api-key"
export QWEN_API_KEY="your-aliyun-api-key"
export MOONSHOT_API_KEY="your-moonshot-api-key"

# Default models (overrides config file)
export DEEPSEEK_API_MODEL="deepseek-chat"
export OPENAI_API_MODEL="gpt-4o-mini"

# Proxy (per provider)
export DEEPSEEK_API_PROXY="http://127.0.0.1:8080"
```

### Command-Line API Key & Proxy

```bash
# Global API key
ai chat -k "sk-..." "Hello"

# Global proxy
ai chat -x "http://127.0.0.1:8080" "Hello"
```

## Tools

`ai` can call tools on behalf of the AI. All tool categories are enabled by default. The following categories are available:

### execute

Shell and command execution. All execute tools support `requires_confirmation` (bool), optional `timeout` (integer, seconds), optional `working_directory`, and optional output `filter` (ordered array of `head`/`tail`/`include`/`exclude` filters).

| Function          | Description                                                                                                                                                                          | Availability                               |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------ |
| `bash`            | Execute arbitrary bash commands via `bash -c`. Full shell features (pipes, redirects, variable expansion)                                                                            | Linux/macOS; Windows if `bash.exe` in PATH |
| `cmd`             | Execute Windows CMD commands via `cmd /d /s /c`                                                                                                                                      | Windows only                               |
| `powershell`      | Execute PowerShell commands via `powershell -NoProfile -Command`                                                                                                                     | Windows only                               |
| `execute_command` | Execute a binary directly via exec/CreateProcessW (no shell). Paths containing `/` or `\` are treated as file paths; otherwise resolved from PATH. Returns exit code, stdout, stderr | All platforms                              |

### filesystem

File and directory operations.

| Function              | Description                                                                      |
| --------------------- | -------------------------------------------------------------------------------- |
| `read_file`           | Read a file's contents, with optional `offset` and `limit` for chunked reading   |
| `read_multiple_files` | Read multiple files at once                                                      |
| `write_file`          | Create or overwrite a file                                                       |
| `edit_file`           | Apply SEARCH/REPLACE blocks to edit a file                                       |
| `replace_lines`       | Replace a specific range of lines (1-indexed) in a file                          |
| `create_directory`    | Create directories (including nested ones)                                       |
| `list_directory`      | List files and directories at a given path                                       |
| `directory_tree`      | Get a recursive JSON tree view of a directory                                    |
| `find_files`          | Recursively search for files matching a glob pattern (case-insensitive)          |
| `move_file`           | Move or rename files and directories                                             |
| `get_file_info`       | Get detailed metadata (size, creation time, modified time, permissions, type)    |
| `disk_space_info`     | Get disk capacity, free space, available space, used space, and usage percentage |

### default

Session and environment queries.

| Function                   | Description                            |
| -------------------------- | -------------------------------------- |
| `get_working_directory`    | Get the current working directory      |
| `get_environment_variable` | Read an environment variable           |
| `set_environment_variable` | Set an environment variable            |
| `get_shell`                | Get the default shell path             |
| `get_operating_system`     | Get OS name, version, and architecture |

### interactive

User interaction (only available when running with a TTY).

| Function   | Description                                                    |
| ---------- | -------------------------------------------------------------- |
| `ask_user` | Prompt the user with a question and a numbered menu of options |

> **Note:** By default, all tool categories are enabled. Use `--no-tools` to disable all tools, or `--tools execute,filesystem` to enable only specific categories.

## Complete Command Line Options

```
% ai --help
OpenAI API-compatible multi-provider CLI chatbot with tool-calling capabilities

Usage:
  ai [options]... [cmd] [options]...

Options:
 -v, --version                   Print version information and exit
 -x, --proxy <PROXY>             HTTP/HTTPS proxy URL for API requests
 -k, --key <KEY>                 API key used for authenticating with the AI provider
     --log-level <0..4>          Set logging verbosity level (lower values are more verbose)
                                 [0:DEBUG, 1:INFO, 2:WARNING, 3:ERROR, 4:FATAL]
     --verbose                   Same as --log-level -1 (maximum verbosity)
     --debug                     Same as --log-level 0
     --info                      Same as --log-level 1
     --warn                      Same as --log-level 2
     --error                     Same as --log-level 3
     --fatal                     Same as --log-level 4
     --log-to <arg>              Path to the log file

Available Commands:
 chat                            Start an interactive chat session with the AI assistant
 models                          List available AI models
 history                         List recent chat session history
 update                          Check for a newer version on GitHub and self-update if available

% ai chat --help
ai chat [options]... <prompts>

Options:
     --[no-]stream               Enable streaming mode (tokens displayed as generated)
     --[no-]stream-include-usage Include token usage statistics at end of streaming output
 -C                              Continue conversation from the last saved chat history
     --continue-from <SESSION_ID>
                                 Continue conversation from a specific session ID
 -m, --model <arg>               AI model name (e.g., gpt-4o, deepseek-chat)
 -s, --system-prompt <arg>       System prompt for setting behavior/context
 -t, --temperature <0.0..2.0>    Sampling temperature
     --top-p <0.0..1.0>          Nucleus sampling parameter
 -u, --url <arg>                 OpenAI API-compatible base URL
     --base-url <arg>            OpenAI API-compatible base URL (appends /chat/completions)
     --max-tokens <N>            Maximum tokens to generate
     --reasoning-effort <arg>    Reasoning depth [low, medium, high, none]
     --[no-]thinking             Enable or disable AI thinking
     --no-tools                  Disable all tool calling capabilities
     --list-tools                List all available tool categories and their functions
     --tools <arg>               Tool categories to enable [execute, filesystem, default, interactive]
                                 (default: all categories)
     --tool-choice <arg>         Tool selection strategy [none, auto, required]
     --deepseek                  Use DeepSeek API
     --openai                    Use OpenAI API
     --gemini                    Use Gemini API
     --google                    Same as --gemini
     --qwen                      Use Qwen API
     --moonshot                  Use Moonshot API
     --ollama                    Use Ollama (local)

Positionals:
 prompts                        User message(s); use '-' to read from stdin

% ai models --help
ai models [options]...

Options:
 -k, --key <key>                 OpenAI API key
 -u, --url <arg>                 OpenAI API-compatible base URL
     --base-url <arg>            OpenAI API-compatible base URL (appends /models)
     --deepseek                  Use DeepSeek API
     --openai                    Use OpenAI API
     --gemini                    Use Gemini API
     --google                    Same as --gemini
     --qwen                      Use Qwen API
     --moonshot                  Use Moonshot API
     --ollama                    Use Ollama (local)

% ai history --help
ai history [options]...

Options:
     --limit <0..N>              Number of recent sessions to list (0 for all, default: 20)
     --json <FIELDS>             Output as JSON array (comma-separated: session-id, start, end, topic, messages)
     --text                      Output in detailed human-readable text format
     --line <FIELDS>             Output as pipe-delimited lines (comma-separated: session_id, start, work_dir, topic, messages)
     --session <SESSION_ID>      Print the full conversation for a specific session ID

% ai update --help
ai update [options]...

Options:
 -f, --force                    Force update even if already on the latest version
```

### Shell Completions

```bash
# bash
eval "$(ai --print-bash-complete)"

# zsh
eval "$(ai --print-zsh-complete)"

# fish
ai --print-fish-complete | source
```

## License

MIT License

## Docs

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/shediao/ai.cli)
