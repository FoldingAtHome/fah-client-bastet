# Contributing to Folding@home Client

Thank you for your interest in contributing to the Folding@home client! This document provides guidelines and information to help you contribute effectively.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Building the Project](#building-the-project)
- [Code Style Guidelines](#code-style-guidelines)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Architecture Overview](#architecture-overview)
- [Reporting Bugs](#reporting-bugs)

## Getting Started

### Prerequisites

- **C++ compiler**: GCC, Clang, or MSVC
- **SCons**: Build system (version 3.0+)
- **cbang library**: The client depends on [cbang](https://github.com/cauldrondevelopmentllc/cbang)
- **Git**: For version control

### Platform-Specific Requirements

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install -y scons git npm build-essential fakeroot libssl-dev zlib1g-dev libbz2-dev liblz4-dev libsystemd-dev
```

**Fedora/RHEL:**
```bash
sudo dnf -y install git python3-scons gcc-c++ openssl-devel zlib-devel bzip2-devel lz4-devel systemd-devel rpm-build systemd-rpm-macros
```

**openSUSE:**
```bash
sudo zypper -n in git scons gcc-c++ libopenssl-devel zlib-devel libbz2-devel liblz4-devel systemd-devel rpm-build systemd-rpm-macros
```

**macOS:**
```bash
brew install scons
```

**Windows:**
- Install [MSYS2](https://www.msys2.org/) or use Visual Studio with MSVC
- Install Python and SCons

## Development Setup

1. **Clone the repositories:**
```bash
git clone https://github.com/cauldrondevelopmentllc/cbang
git clone https://github.com/foldingathome/fah-client-bastet
git clone https://github.com/foldingathome/fah-web-client-bastet  # Optional, for frontend development
```

2. **Set up the cbang dependency:**
```bash
export CBANG_HOME=$PWD/cbang
scons -C cbang
```

3. **Build the client:**
```bash
scons -C fah-client-bastet
```

4. **Run the client (for testing):**
```bash
cd fah-client-bastet
./fah-client --help
```

## Building the Project

### Standard Build
```bash
export CBANG_HOME=$PWD/cbang
scons -C cbang
scons -C fah-client-bastet
```

### Building Packages

**Debian/Ubuntu:**
```bash
scons -C fah-client-bastet package
```

**RPM/Fedora:**
```bash
scons -C fah-client-bastet package
```

**macOS:**
```bash
scons -C fah-client-bastet package
```

### Build Options

The SConstruct file supports various build options:
- `wrap_glibc`: Wrap glibc functions for backwards compatibility with older Linux
- `cross_mingw`: Cross-compile for Windows using MinGW

View all options:
```bash
scons -C fah-client-bastet --help
```

## Code Style Guidelines

### C++ Code Style

The codebase follows these conventions:

1. **File Headers**: All source files must include the GPL license header
2. **Namespace Usage**: Use `using namespace` declarations at the top of .cpp files:
   ```cpp
   using namespace FAH::Client;
   using namespace cb;
   using namespace std;
   ```
3. **Naming Conventions**:
   - Classes: PascalCase (e.g., `GPUResources`)
   - Methods: camelCase (e.g., `isDetected()`)
   - Member variables: camelCase
   - Constants: UPPER_SNAKE_CASE
4. **Include Guards**: Use `#pragma once` for header files
5. **Brace Style**: K&R style (opening brace on same line)
6. **Indentation**: 2 spaces

### Comment Guidelines

- **DO NOT add or remove comments unless asked** (per project policy)
- Existing comments should be preserved during edits
- If you accidentally delete a comment, restore it

### Commit Messages

Follow conventional commit format:
```
<type>: <subject>

<body>

<footer>
```

Types: `fix`, `feat`, `docs`, `style`, `refactor`, `test`, `chore`

Example:
```
fix: Correct ARM64 architecture detection in LinOSImpl

The CPU architecture check was missing the 'a' prefix in 'aarch64',
causing incorrect CPU detection on ARM64 systems.
```

## Testing

### Running Unit Tests

The project includes unit tests in the `tests/` directory:

```bash
cd tests
scons
```

### Manual Testing

1. **Hardware Detection**: Use the `fahctl` script to check GPU detection:
```bash
./scripts/fahctl state
```

2. **Work Unit Processing**: Monitor log files in `/var/log/fah-client/` (Linux) or the data directory

3. **Frontend Integration**: Test with the web frontend:
```bash
cd ../fah-web-client-bastet
npm i
npm run dev
```

### Test Coverage

Critical areas that should be tested (see `tests/TESTING.md` for details):
- Unit state machine progression
- Retry and backoff behavior
- GPU detection and compute device support
- Config merge logic
- Persistence across restarts

## Submitting Changes

### Pull Request Process

1. **Fork the repository** on GitHub
2. **Create a feature branch**:
```bash
git checkout -b feature/your-feature-name
```
3. **Make your changes** following the code style guidelines
4. **Build and test** your changes
5. **Commit your changes** with clear messages
6. **Push to your fork**:
```bash
git push origin feature/your-feature-name
```
7. **Create a Pull Request** on GitHub

### Pull Request Checklist

Before submitting, ensure:
- [ ] Code follows the project's style guidelines
- [ ] Build succeeds on at least one platform
- [ ] Tests pass (if applicable)
- [ ] Documentation is updated (if needed)
- [ ] Commit messages are clear and descriptive
- [ ] No unnecessary files are included (build artifacts, etc.)

### Code Review

- Be responsive to review feedback
- Address all requested changes
- Keep discussions constructive and focused on the code

## Architecture Overview

Before making non-trivial changes, read [ARCHITECTURE.md](ARCHITECTURE.md) for a detailed understanding of:

- Major components (App, Server, Remote, Groups, Units, etc.)
- Event flow and state machines
- Hardware detection system
- Persistence and database handling

### Key Components

- **App**: Singleton managing the client lifecycle
- **Unit**: Work unit state machine (ASSIGN → DOWNLOAD → CORE → RUN → UPLOAD → DONE)
- **Group**: Resource pool with its own configuration
- **GPUResources**: Hardware detection via PCI, CUDA, HIP, OpenCL
- **Server**: HTTP/WebSocket server for frontend connections

### Hardware Detection

The client detects GPUs using the cbang library's GPUIndex:
- Downloads GPU database from `https://api.foldingathome.org/gpus`
- Caches as `gpus.json` (updated every 5 days)
- Retries GPU detection for up to 5 minutes after startup (for driver loading)
- Supports CUDA, HIP, and OpenCL compute devices

## Reporting Bugs

When reporting bugs, please:

1. **Check existing issues** first to avoid duplicates
2. **Use a descriptive title** that summarizes the problem
3. **Include the following information**:
   - Operating system and version
   - Client version (from `fah-client --version`)
   - Browser (if frontend-related)
   - Expected behavior
   - Actual behavior
   - Steps to reproduce
   - Relevant log excerpts (redact sensitive information)
   - Hardware details (CPU, GPU, drivers)

4. **Classify the issue** as frontend or backend to file in the appropriate repository:
   - Backend: [fah-client-bastet](https://github.com/FoldingAtHome/fah-client-bastet)
   - Frontend: [fah-web-client-bastet](https://github.com/FoldingAtHome/fah-web-client-bastet)

## Communication

- **GitHub Issues**: For bug reports and feature requests
- **Pull Requests**: For code contributions
- **Slack**: For real-time communication (invite-only)

## Resources

- [Architecture Documentation](ARCHITECTURE.md)
- [Building Guide (Debian)](README.md)
- [Building Guide (RPM)](BUILDING-RPM.md)
- [Testing Guide](TESTING.md)
- [Changelog](CHANGELOG.md)
- [Folding@home Website](https://foldingathome.org/)

## License

By contributing, you agree that your contributions will be licensed under the [GPL-3.0-or-later](LICENSE) license.

## Security

If you discover a security vulnerability, please report it privately via:
- Email: security@foldingathome.org
- Do not create a public issue for security vulnerabilities

Thank you for contributing to Folding@home!
