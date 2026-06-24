# Contributing to OmniPlexus

Thank you for your interest in contributing to OmniPlexus. We welcome contributions of all kinds — bug reports, documentation improvements, tests, firmware, C++ library code, hardware schematics, and mechanical CAD designs. This document explains the standard contribution process and the project-specific rules you must follow.

---

## Table of Contents

1. [Licensing and Developer Certificate of Origin](#1-licensing-and-developer-certificate-of-origin)
2. [Attribution](#2-attribution)
3. [Naming Conventions](#3-naming-conventions)
4. [Documentation Standards (Doxygen)](#4-documentation-standards-doxygen)
5. [Coding Style and Formatting](#5-coding-style-and-formatting)
6. [How to Contribute](#6-how-to-contribute)
7. [Pull Request Guidelines](#7-pull-request-guidelines)
8. [Tests and Continuous Integration](#8-tests-and-continuous-integration)
9. [Security Issues](#9-security-issues)
10. [Code of Conduct](#10-code-of-conduct)

---

## 1. Licensing and Developer Certificate of Origin

Unlike some projects, we do not require you to assign or transfer copyright ownership to a central entity. You retain full ownership of your original work.

However, by submitting a Pull Request or patch to OmniPlexus, you certify that:

- You have the legal right to contribute the code or hardware design under the terms below, and
- You grant the project maintainers a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to reproduce, prepare derivative works of, publicly display, perform, and distribute your contribution under the project's open licenses.

Contributions are licensed as follows:

| Contribution Type | License |
|---|---|
| Software (C++, firmware, APIs, tooling) | [Mozilla Public License 2.0 (MPL-2.0)](https://www.mozilla.org/en-US/MPL/2.0/) |
| Hardware (PCB, mechanical CAD, OpenSCAD) | [CERN Open Hardware Licence v2 – Weakly Reciprocal (CERN-OHL-W)](https://ohwr.org/cern_ohl_w_v2.txt) |

By submitting a contribution, you acknowledge and agree to these terms. This ensures OmniPlexus can remain open and independently sustainable.

---

## 2. Attribution

We maintain a public record of all contributors. Unless you explicitly request otherwise in writing, your GitHub username and any name associated with your commits will appear in the project history and contributor lists.

---

## 3. Naming Conventions

### Software (C++)

| Scope | Convention | Example |
|---|---|---|
| Classes and structs | `PascalCase` | `CommunicationManager`, `BinaryEncoder`, `TaggedFrame` |
| Interface classes | `I` prefix + `PascalCase` | `ITransport`, `IEncoder`, `IMutex` |
| Namespaces | `PascalCase` | `ProtocolConstants` |
| Public and private methods | `camelCase` | `sendCommand()`, `beginSerial()`, `hasCompleteFrame()` |
| `using` type aliases | `PascalCase` | `CommandHandler`, `TelemetryCallback` |
| `enum class` type names | `PascalCase` | `FrameType`, `LogLevel`, `ResponseStatus` |
| `enum class` values | `UPPER_SNAKE_CASE` | `OP_WARNING`, `UNKNOWN_COMMAND_TYPE` |
| Local variables and member variables | `camelCase`, no leading underscore | `frameBuffer`, `heartbeatTimeoutMs`, `retryCount` |
| Preprocessor macros and compile-time constants | `UPPER_SNAKE_CASE` | `OPX_MAX_DEVICES`, `MAX_TRANSPORTS` |

### Hardware (OpenSCAD)

Parameterized variables in OpenSCAD scripts must use `snake_case` for readability and consistency with OpenSCAD community conventions. Private or internal identifiers may follow project or language idioms, but consistency within the same file is required.

---

## 4. Documentation Standards (Doxygen)

Every public C++ class and function **must** be documented using Doxygen-style comments. At minimum, documentation must include:

- A brief description of the purpose.
- Descriptions of parameters and return values (unless clearly self-explanatory from the description).
- Any relevant side effects, ownership rules, thread-safety notes, or important invariants.

### Block Comment Style (preferred for C++)

```cpp
/**
 * @brief Calculates the required torque for the specified joint.
 *
 * @param[in] mass The mass of the attached load in kilograms.
 * @return The required torque in Newton-meters.
 */
float calculateTorque(float mass);
```

### Triple-Slash Style (acceptable alternative)

```cpp
/// @brief Calculates the required torque for the specified joint.
/// @param[in] mass The mass of the attached load in kilograms.
/// @return The required torque in Newton-meters.
float calculateTorque(float mass);
```

### Class-Level Documentation

Class declarations must include a Doxygen summary and any important usage or safety remarks.

```cpp
/**
 * @brief Manages the master I²C bus for the sensor array.
 *
 * @note This class is not thread-safe. Callers are responsible
 *       for acquiring the appropriate mutex before use.
 */
class SensorBus { /* ... */ };
```

### Examples

If your contribution introduces a new API, add a corresponding example to the `examples/` directory. Examples should be extensively commented so they can serve as a learning resource.

---

## 5. Coding Style and Formatting

OmniPlexus targets both PC and resource-constrained embedded platforms (ESP32, STM32, Arduino/AVR). Code must respect the following rules.

### Fixed-Width Integer Types

Always use `<cstdint>` types. Never use bare language primitives where width matters.

```cpp
// Correct
uint16_t commandId;
int32_t  motorSpeed;

// Incorrect
unsigned short commandId;
int            motorSpeed;
```

### Const Correctness

Mark any value, reference, or pointer that must not change as `const`.

```cpp
void processFrame(const uint8_t* data, uint16_t length);
```

### No Heap Allocation in the Main Loop

Pre-allocate all objects during initialization. Do not call `new` or `delete` (or `malloc` / `free`) while the robot is actively running. Heap fragmentation on microcontrollers leads to silent crashes.

### Hardware Abstraction

Library code must never call platform APIs (`Serial`, `WiFi`, HAL drivers, etc.) directly. All I/O must go through the `ITransport` interface so the library remains portable across targets.

### Memory Layout

Structs shared across the wire must be annotated with `#pragma pack(1)` to guarantee binary compatibility between processors with different alignment requirements (e.g., a 64-bit PC and a 32-bit STM32).

```cpp
#pragma pack(push, 1)
struct CommandFrame {
    uint16_t commandType;
    float    w, x, y, z;
    int16_t  s, t, u, v;
};
#pragma pack(pop)
```

### Formatting

Follow the style of the file you are editing. If a `.clang-format` or `.editorconfig` file is present in the repository, run the formatter before committing. Do not reformat unrelated code in your PR — it makes diffs harder to review.

---

## 6. How to Contribute

1. **Fork** the repository and create a branch with a descriptive name:
   - `feature/wifi-transport-reconnect`
   - `fix/crc8-overflow-avr`
   - `docs/update-discovery-protocol`

2. **Implement** your change, following the naming, documentation, and coding rules in this document.

3. **Test** your change. For firmware, verify on real hardware where possible. For hardware designs, ensure KiCad DRC/ERC checks or CAD interference analyses pass before submitting.

4. **Run** any formatters and linters locally and confirm they pass.

5. **Commit** with a clear, descriptive message:
   ```
   fix(transport): handle TCP reconnect after timeout on PC side
   ```

6. **Push** your branch to your fork and **open a Pull Request** against the `main` branch of the OmniPlexus repository. Include a description of the change, the motivation, and references to any relevant issues. Attach screenshots, terminal logs, or CAD renders where they help reviewers.

---

## 7. Pull Request Guidelines

- **One logical change per PR.** Atomic PRs are faster to review and easier to revert if needed.
- **Describe the problem, the solution, and alternatives considered.**
- **Reference related issues** (e.g., `Fixes #123`, `Closes #456`).
- **Ensure CI passes** on all checks before requesting a review.
- **Do not mix formatting changes with functional changes.** If you need to reformat a file, do it in a separate, dedicated commit or PR.

---

## 8. Tests and Continuous Integration

Any contribution that changes behavior must include tests — unit tests where the logic can be isolated, or documented manual hardware test procedures where it cannot. New tests must pass both locally and in CI before a PR can be merged.

If your change requires a modification to the CI configuration (e.g., adding a new embedded target toolchain), explain the rationale clearly in the PR description.

---

## 9. Security Issues

If you discover a security vulnerability, **do not open a public issue.** Contact the maintainers privately using the security contact listed in the repository settings so we can investigate and coordinate responsible disclosure before any public announcement.

---

## 10. Code of Conduct

By participating in this project, you agree to abide by the project's Code of Conduct. Please see `CODE_OF_CONDUCT.md` for details. If no such file exists, follow common standards of respect and professionalism: be constructive, be inclusive, and assume good faith.

---

## Thank You

Your contributions make OmniPlexus better for every developer who builds on it. We appreciate the time you invest in improving the framework.

— *The OmniPlexus Maintainers*
