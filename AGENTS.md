# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

## Project Overview

This is the **Folding@home Client Backend (Bastet)** — a C++ distributed computing client that runs protein folding simulations. It is the backend half of the FAH v8 client; the frontend lives in a separate repository ([fah-web-client-bastet](https://github.com/foldingathome/fah-web-client-bastet)).

## Build Commands

The project uses **SCons** (Python-based build system) and depends on the **cbang** C++ library, which must be built first and pointed to via `CBANG_HOME`.

```bash
# Prerequisites (Debian/Ubuntu)
sudo apt install scons git npm build-essential fakeroot libssl-dev zlib1g-dev libbz2-dev liblz4-dev libsystemd-dev

# Prerequisites (macOS with Homebrew)
brew install scons openssl@3 lz4

# Build cbang first (sibling directory)
export CBANG_HOME=$PWD/../cbang
# On macOS (Homebrew), OpenSSL is not in the default search path:
export OPENSSL_HOME=$(brew --prefix openssl@3)
scons -C $CBANG_HOME

# Build the client
scons

# Build a distribution package (.deb, .rpm, .pkg, or .exe depending on platform)
scons package
```

**Note:** On macOS with Homebrew (especially Apple Silicon), `OPENSSL_HOME` must be set as an environment variable for both the cbang and client builds. The cbang build system uses `OPENSSL_HOME` to locate headers and libraries. If cbang is built without it, OpenSSL support will be silently omitted, causing compile errors in the client.

There is no automated test suite — testing is manual/integration-based (see TESTING.md).

## Architecture

The application is event-driven, built on the **cbang** framework which provides the application lifecycle, event loop, HTTP client/server, JSON handling, database, SSL/TLS, subprocess management, and more.

### Key Components (all under `src/fah/client/`)

- **App** — Central application class. Extends `cb::Application` and `cb::JSON::ObservableDict`. Owns the event loop, HTTP client, SQLite database, and coordinates all subsystems.
- **Config** / **Account** — User configuration and account authentication (via JSON WebSocket to FAH servers).
- **Unit** / **Units** — Work unit (protein folding task) lifecycle management. `Unit` is the largest and most complex component.
- **Group** / **Groups** — Resource groups that organize how CPU/GPU resources are allocated to folding work.
- **GPUResource** / **GPUResources** — GPU inventory and management.
- **Core** / **Cores** / **CoreProcess** — Management of simulation engine binaries ("cores") that run as subprocesses.
- **Server** — Local HTTP server for the web frontend to connect to.
- **Remote** / **WebsocketRemote** / **NodeRemote** — Remote control interfaces.
- **OS** — Platform abstraction with implementations in `lin/`, `osx/`, `win/` subdirectories.

### Namespace and Structure

All code is in the `FAH::Client` namespace. The single entry point is `src/client.cpp` which instantiates `App` and calls `start()`. Resources (CA cert, JSON schemas) are compiled in via `src/resources/` and accessed through `FAH::Client::Resource`.

### Platform-Specific Code

Platform implementations live in subdirectories and are conditionally compiled via `client.scons`:
- `src/fah/client/lin/` — Linux (systemd, udev GPU detection)
- `src/fah/client/osx/` — macOS (Cocoa APIs, Launch Agents)
- `src/fah/client/win/` — Windows (Win32 API, system tray)

### Build System Details

- `SConstruct` — Top-level build config. Reads version from `package.json`. Handles platform detection, packaging for deb/rpm/pkg/nsis.
- `src/client.scons` — Compiles all `.cpp` files in `src/fah/client/`, plus platform-specific subdirectory, plus generated resources and build info.
- Build output goes to `build/` directory. The resulting binary is `fah-client` at the project root.
- `wrap_glibc` option wraps certain glibc symbols for backward compatibility on older Linux.

## Code Conventions

Follow coding style guidelines found here: 
[C! coding style guide](https://github.com/cauldrondevelopmentllc/cbang/blob/master/CODING_STYLE.md).

- Uses `cb::SmartPointer<T>` (from cbang) rather than `std::shared_ptr`.
- Observable pattern via `cb::JSON::ObservableDict` — config/state changes propagate through JSON notifications.
- RAII throughout; event-driven control flow via `cb::Event::Base`.
- GPL-3.0 license header required on all source files.

## Commit Style

This project uses [Conventional Commits](https://www.conventionalcommits.org/). Format:

```
<type>(<optional scope>): <description>

<optional body>
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`, `style`.
