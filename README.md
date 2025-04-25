# OpenAI CLI

A powerful command-line tool for interacting with the OpenAI API. Supports chat, text generation, code generation, and more.

## Features

- Interactive and non-interactive modes
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
ai chat "Write a quicksort algorithm" --model gpt-4

# Enable streaming output
ai chat "Tell me a story" --stream

# Use system prompt
ai chat "Write code" --system-prompt "You are a professional C++ programmer"
```

### Interactive Mode

```bash
# Start interactive session
ai chat --interactive

# Interactive session with system prompt
ai chat --interactive --system-prompt "You are a friendly AI assistant"
```

### Environment Variables

You can set the API key using environment variables:

```bash
export OPENAI_API_KEY="your-api-key"
ai chat "Hello"
```

### Complete Command Line Options

```
Options:
  -h, --help               Show help information
  --version                Show version information
  -s, --system-prompt      System prompt
  -m, --model              Model to use (default: gpt-3.5-turbo)
  -k, --key                OpenAI API key (set to 'ollama' for ollama service)
  -u, --url                OpenAI API URL (default: https://api.openai.com/v1/chat/completions)
  --stream                 Enable streaming mode
  -t, --temperature        Model temperature (default: 0.1)
  -i, --interactive        Enable interactive mode
  -d, --debug              Enable debug mode
  -v, --verbose            Enable verbose mode
  --top-p                  Model top-p parameter (default: 1.0)
  --proxy                  Use proxy
```

## License

MIT License 
