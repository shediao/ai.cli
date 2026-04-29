# ai

A powerful command-line AI chatbot with multi-provider support and tool-calling capabilities. Uses OpenAI-compatible APIs to interact with DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama, and more.

## Features

- **Multi-provider support** — DeepSeek, OpenAI, Gemini, Qwen, Moonshot, Ollama, and any OpenAI-compatible API via config
- **Tool calling** — AI can execute bash commands, read/write/edit files, run git operations, and more (see [Tools](#tools))
- **Streaming output** — tokens are displayed as they are generated
- **Image input** — attach images via local files or URLs for vision-capable models
- **Chat history** — conversation is persisted to disk; continue previous sessions with `-C`
- **Editor input** — if no prompt is given and stdin is a TTY, opens `$EDITOR` for composing the prompt
- **Pipe / stdin** — read prompts from stdin; use `-` as a positional to read from the pipe
- **Clipboard** — responses are automatically copied to the clipboard
- **Configurable system prompt** — automatic context injection (CWD, git status, directory tree, OS, shell)
- **Shell completions** — built-in generation for bash, zsh, and fish
- **Adjustable parameters** — temperature, top-p, max tokens, reasoning effort
- **Config file** — manage providers, default models, and API keys in `config.json`
- **Logging** — configurable log levels and output destinations (stderr / file / both)

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

| Option | Default | Description |
|--------|---------|-------------|
| `AICLI_BUILD_TESTS` | ON | Build unit tests |
| `AICLI_ENABLE_ASAN` | ON | Enable Address/Undefined Sanitizers in Debug |
| `AICLI_USE_SYSTEM_CURL` | OFF | Use system-installed libcurl instead of fetching it |

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
```

### List Available Models

```bash
# List models from the default provider (deepseek)
ai models

# List models from a specific provider
ai models --openai
ai models --gemini
```

### Disable / Configure Tools

```bash
# Disable all tools
ai chat --no-tools "Hello"

# Enable only specific tool categories
ai chat --tools bash "List files in /tmp"
ai chat --tools filesystem,git "What's the git status?"

# Force a tool call
ai chat --tool-choice required --tools bash "List the current directory"
```

## Configuration

### Config File

`ai` creates a config file at:

- **Linux**: `~/.local/share/ai.cli/config.json`
- **macOS**: `~/Library/Application Support/ai.cli/config.json`
- **Windows**: `%USERPROFILE%\AppData\Local\Shediao\ai.cli\config.json`

The default config:

```json
{
  "providers": [
    { "alias": "deepseek", "base_url": "https://api.deepseek.com",              "default_model": "deepseek-v4-pro" },
    { "alias": "openai",   "base_url": "https://api.openai.com/v1",             "default_model": "gpt-4o" },
    { "alias": "gemini",   "base_url": "https://generativelanguage.googleapis.com/v1beta/openai/", "default_model": "gemini-flash-latest" },
    { "alias": "qwen",     "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1/",       "default_model": "qwen-max-latest" },
    { "alias": "moonshot", "base_url": "https://api.moonshot.cn/v1",            "default_model": "kimi-k2.6" },
    { "alias": "ollama",   "base_url": "http://127.0.0.1:11434/v1" }
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

`ai` can call tools on behalf of the AI. The following tool categories are available:

### bash

| Function | Description |
|----------|-------------|
| `bash` | Execute arbitrary bash commands (with optional user confirmation for destructive commands) |

### filesystem

| Function | Description |
|----------|-------------|
| `read_file` | Read a file's contents, with optional `offset` and `limit` for chunked reading |
| `read_multiple_files` | Read multiple files at once |
| `write_file` | Create or overwrite a file |
| `edit_file` | Apply SEARCH/REPLACE blocks to edit a file (shows diff with delta/diff) |
| `replace_lines` | Replace a specific range of lines (1-indexed) in a file |
| `create_directory` | Create directories (including nested) |
| `list_directory` | List files and directories at a given path |
| `directory_tree` | Get a recursive JSON tree view of a directory |
| `search_files` | Search for files matching a pattern (with glob support) |
| `move_file` | Move or rename files and directories |
| `get_file_info` | Get detailed metadata (size, type, permissions, modified time) |
| `disk_space_info` | Get disk capacity, free space, and usage percentage |
| `execute_file` | Execute a file as a subprocess and capture exit code, stdout, and stderr |

### git

| Function | Description |
|----------|-------------|
| `git_status` | Show working tree status (porcelain format) |
| `git_diff` | Show changes (staged/unstaged, commit ranges, file-specific) |
| `git_log` | Show commit history (oneline or detailed, configurable count) |
| `git_add` | Stage files for commit |
| `git_commit` | Create a commit with a message |
| `git_branch` | List/create/delete branches (local and remote) |
| `git_checkout` | Switch branches or restore files |
| `git_init` | Initialize a new git repository |
| `git_clone` | Clone a remote repository |

### default

| Function | Description |
|----------|-------------|
| `get_working_directory` | Get the current working directory |
| `set_working_directory` | Change the current working directory |
| `get_environment_variable` | Read an environment variable |
| `set_environment_variable` | Set an environment variable |
| `get_shell` | Get the default shell path |
| `get_operating_system` | Get OS name, architecture, and version |

> **Note:** By default, `bash` and `filesystem` tools are enabled. Enable `git` or `default` with `--tools git,default`. Disable all tools with `--no-tools`.

## Complete Command Line Options

```
% ai --help
OpenAI API-compatible multi-provider CLI chatbot with tool-calling capabilities

Usage:
  ai [options]... [cmd] [options]...

Options:
     --version                   Print version information and exit
 -x, --proxy <PROXY>             HTTP/HTTPS proxy URL for API requests
 -k, --key <KEY>                 API key used for authenticating with the AI provider
     --log-level <0..4>          Set logging verbosity level (lower values are more verbose)
                                 [0:DEBUG, 1:INFO, 2:WARNING, 3:ERROR, 4:FATAL]
 -v                              Decrease log verbosity
     --enable-logging <arg>      Enable logging and choose output destination
                                 [file, stderr, all] (default: stderr)
     --log-file <arg>            Path to log file (default: debug.log)

Available Commands:
 chat                            Start an interactive chat session with the AI assistant
 models                          List available AI models

% ai chat --help
ai chat [options]... <prompts>

Options:
     --[no-]stream               Enable streaming mode (tokens displayed as generated)
     --[no-]stream-include-usage Include token usage statistics at end of streaming output
 -C                              Continue conversation from the last saved chat history
 -m, --model <arg>               AI model name (e.g., gpt-4o, deepseek-chat)
 -s, --system-prompt <arg>       System prompt for setting behavior/context
 -t, --temperature <0.0..2.0>    Sampling temperature
     --top-p <0.0..1.0>          Nucleus sampling parameter
 -u, --url <arg>                 OpenAI API-compatible base URL
     --base-url <arg>            OpenAI API-compatible base URL (appends /chat/completions)
     --max-tokens <N>            Maximum tokens to generate
     --reasoning-effort <arg>    Reasoning depth [low, medium, high, none]
     --no-tools                  Disable all tool calling capabilities
     --tools <arg>               Tool categories to enable [bash, filesystem, git, default]
                                 (default: bash, filesystem)
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
