# OpenAI CLI

A powerful command-line tool for interacting with the OpenAI API. Supports chat, text generation, code generation, and more.

## Features

- Streaming output support
- Configurable system prompts
- Support for multiple OpenAI models
- Adjustable model parameters (temperature, top-p, etc.)
- API key configuration via environment variables or command line parameters
- Detailed debug output options

## Build Requirements

- C++20 compatible compiler
- CMake 3.20 or higher
- libcurl
- Network connection for downloading dependencies

## Build Steps

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

### Basic Usage

```bash
# Show help information
ai --help
ai chat --help

# Send a simple prompt
ai chat "Hello, World!"

# Use a specific model
ai chat --model deepseek-r1 "Write a quicksort algorithm"

# Enable streaming output
ai chat --stream "Tell me a story"

# Use system prompt
ai chat --system-prompt "You are a professional C++ programmer" "Write code"
```

### Environment Variables

You can set the API key using environment variables:

```bash
export OPENAI_API_KEY="your-openai-api-key"
export DEEPSEEK_API_KEY="your-deepseek-api-key"
export GEMINI_API_KEY="your-google-api-key"
export QWEN_API_KEY="your-aliyun-api-key"
export MOONSHOT_API_KEY="your-moonshot-api-key"
ai chat --openai   "Hello ChatGPT"
ai chat --deepseek "Hello DeepSeek"
ai chat --gemini   "Hello Google"
ai chat --qwen     "Hello Qwen"
ai chat --moonshot "Hello kimi"
ai chat --ollama   "Hello ollama"

```

### Complete Command Line Options

```
% ai --help
OpenAI API Compatible Command Line Chatbot

Usage:
  ai [options]... [cmd] [options]...

Options:
 -h, --help                      show this help info
 -d, --[no-]debug                Enable debug mode
     --proxy <arg>               Use proxy

Available Commands:
 chat                            ai chatbot
 models                          list models

% ai chat --help
ai chat [options]... <prompts>

Options:
     --[no-]stream               Enable streaming mode
     --[no-]stream-include-usage print usage in streaming mode
 -v, --verbose                   Enable verbose mode
     --version                   Show version
 -k, --key <key>                 OpenAI API key
 -m, --model <arg>               Model to use
 -p, --prompt <arg>              Prompt
 -s, --system-prompt <arg>       System prompt
 -t, --temperature <0.0>         Model temperature
     --top-p <0.0>               Model top-p parameter
 -u, --url <arg>                 OpenAI API Compatible URL
     --base-url <arg>            OpenAI API Compatible URL(<base_url>/chat/completions)
                                 (default:https://api.deepseek.com/)
     --max-tokens <N>            max tokens
 -f, --file <arg>                image file/url
     --reasoning-effort <arg>    reasoning effort
                                 [low,medium,high]
     --qwen                      same as --base-url https://dashscope.aliyuncs.com/compatible-mode/v1/
     --gemini                    same as --base-url https://generativelanguage.googleapis.com/v1beta/openai/
     --google                    same as --base-url https://generativelanguage.googleapis.com/v1beta/openai/
     --deepseek                  same as --base-url https://api.deepseek.com/
     --openai                    same as --base-url https://api.openai.com/v1/
     --moonshot                  same as --base-url https://api.moonshot.cn/v1
     --ollama                    same as --base-url http://127.0.0.1:11434/v1

Positionals:
 prompts                         Prompt

% ai models --help
ai models [options]...

Options:
 -k, --key <key>                 OpenAI API key
 -u, --url <arg>                 OpenAI API Compatible URL
     --base-url <arg>            OpenAI API Compatible URL(<base_url>/models)
                                 (default:https://api.deepseek.com/)
     --qwen                      same as --base-url https://dashscope.aliyuncs.com/compatible-mode/v1/
     --gemini                    same as --base-url https://generativelanguage.googleapis.com/v1beta/openai/
     --google                    same as --base-url https://generativelanguage.googleapis.com/v1beta/openai/
     --deepseek                  same as --base-url https://api.deepseek.com/
     --openai                    same as --base-url https://api.openai.com/v1/
     --moonshot                  same as --base-url https://api.moonshot.cn/v1
     --ollama                    same as --base-url http://127.0.0.1:11434/v1
```

## License

MIT License

## docs
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/shediao/ai.cli)
