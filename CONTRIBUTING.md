# Contributing to Folding@home Client

This guide is for human contributors.
AI coding agents should refer to [AGENTS.md](AGENTS.md) instead.

Thank you for your interest in contributing to Folding@home.
Every improvement to the client helps turn volunteer compute into completed science —
whether that's a bug fix, a documentation update, or testing on your hardware.

## Ways to Contribute

### Code

Fix bugs, improve diagnostics, add platform support, or help with packaging.

### Testing

Run pre-release builds and report issues.
See [TESTING.md](TESTING.md) for the tester guide,
including how to run the client without the frontend and how to file useful bug reports.
Bleeding-edge builds are available at <https://master.foldingathome.org/builds/fah-client/>.

### Documentation

Improve READMEs, installation guides, or inline code comments.
Clear docs reduce support burden and help new contributors onboard faster.

### Bug Reports

File issues on the correct repository:

- **Backend (this repo):** [fah-client-bastet](https://github.com/FoldingAtHome/fah-client-bastet/issues)
- **Frontend:** [fah-web-client-bastet](https://github.com/FoldingAtHome/fah-web-client-bastet/issues)

A good bug report includes:

- A descriptive title
  (e.g., "GPU not detected on AMD RX 7900 with ROCm 6.1" rather than "GPU doesn't work")
- Your OS, hardware, and client version
- What you expected vs. what happened
- Steps to reproduce
- Relevant log output
  (logs are in `/var/log/fah-client` when running as a service,
  or `log.txt` in the working directory otherwise)

## Getting Started

### Build the Client

Quick start:

```bash
# Install prerequisites (Debian/Ubuntu)
sudo apt install scons git npm build-essential \
  libssl-dev zlib1g-dev libbz2-dev liblz4-dev libsystemd-dev

# On macOS: brew install scons openssl@3 lz4
# and set: export OPENSSL_HOME=$(brew --prefix openssl@3)

# Clone and build
git clone https://github.com/cauldrondevelopmentllc/cbang
git clone https://github.com/FoldingAtHome/fah-client-bastet
export CBANG_HOME=$PWD/cbang
scons -C cbang -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
scons -C fah-client-bastet -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Submit a Change

1. Fork the repository and create a branch from `master`.
2. Make your changes.
   Ensure every source file has the GPL-3.0 license header.
3. Build and test locally.
4. Submit a pull request with a clear description of what changed and why.

### AI-Assisted Contributions

AI coding tools can be helpful for writing code and documentation,
but the submitter is responsible for the result.
Pull requests containing code or documentation that the author does not understand
and has not proofread will not be accepted.

### Commit Messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<optional scope>): <description>

<optional body>
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`, `style`.

Examples:

```text
fix(gpu): detect AMD GPUs using HIP when OpenCL is unavailable
docs: add macOS build instructions to AGENTS.md
refactor(unit): extract retry policy into standalone function
```

## Code Guidelines

This project follows the [C! coding style guide](https://github.com/cauldrondevelopmentllc/cbang/blob/master/CODING_STYLE.md).
Additional project-specific rules:

- All code lives in the `FAH::Client` namespace.
- GPL-3.0 license header required on every source file.
- Platform-specific code goes in `src/fah/client/lin/`, `osx/`, or `win/`.

## Communication

- **GitHub Issues:** Primary channel for bugs and feature requests.
- **Discord:** [discord.gg/foldingathome](https://discord.gg/foldingathome)
- **Forum:** [foldingforum.org](https://foldingforum.org/)

## License

By contributing, you agree that your contributions will be licensed under the
[GNU General Public License v3.0](LICENSE).
