# Getting Started — PC + Embedded (CMake)

This guide is for users who are building both a PC-side application and
embedded firmware using OmniPlexus with CMake.

If you only need to flash a microcontroller and don't need a PC application,
see the [Arduino guide](GETTING_STARTED_ARDUINO.md) instead.

Before continuing, read [OmniPlexus Concepts](CONCEPTS.md) to understand
how the library works.

---

## What you need

- **CMake 3.16** or higher — [cmake.org/download](https://cmake.org/download/)
- **Python 3** — [python.org/downloads](https://www.python.org/downloads/)
- **pyyaml**, **questionary**, **rich** — Python dependencies for the tools
- **Boost** (system component) — [boost.org](https://www.boost.org/)
- **A C++17 compiler** — GCC, Clang, or MSVC
- **Arduino IDE** (1.8.x or 2.x) or **Arduino CLI** — for flashing firmware

---

## Step 1 — Install dependencies

### Python packages

```bash
pip install pyyaml questionary rich
```

(Once you've cloned the repo in [Step 2](#step-2--clone-the-repository), you
can instead run `pip install -r tools/requirements.txt` from `Software/` —
same three packages, listed there for convenience.)

### Boost

**Linux (Debian/Ubuntu/Fedora):**

```bash
# Debian/Ubuntu
sudo apt install libboost-system-dev

# Fedora
sudo dnf install boost-devel
```

**macOS:**

```bash
brew install boost
```

**Windows:**
Download from [boost.org](https://www.boost.org/) and follow the
installation instructions. Make sure `BOOST_ROOT` is set in your environment.

---

## Step 2 — Clone the repository

```bash
git clone https://github.com/CezeriLab-EUL/OmniPlexus.git
cd OmniPlexus/Software
```

---

## Step 3 — Write your device manifests

Create a `manifests/` folder inside `Software/` and add a YAML file for
each microcontroller in your robot:

```
Software/
└── manifests/
    ├── MotorBoard.yaml
    └── SensorBoard.yaml
```

### Option A — Write YAML by hand

A fully documented example is available at
`manifests/examples/ExampleDevice.yaml`. Use it as a reference for the correct
structure and all available options.

Here is a minimal example:

```yaml
version: "1.0"
description: Controls the motor driver
device: MotorBoard
typeShift: 1

commands:
  - id: 1
    name: SET_SPEED
    description: Set motor speed
    acknowledges: false
    params:
      - name: speed
        type: INT16
        required: true
        description: Speed value (-255 to 255)

  - id: 2
    name: STOP
    description: Stop the motor immediately
    acknowledges: false
    params: []

telemetry:
  - id: 1
    name: CURRENT_SPEED
    description: Current motor speed reading
    type: INT16
    trigger:
      type: periodic
      intervalMs: 200

settings:
  - id: 1
    name: MAX_SPEED
    description: Maximum allowed speed (0-255)
    type: UINT8
```

### Option B — Author it interactively

```bash
cd tools
python -m ManifestAuthoring ../manifests
```

This walks you through the same fields interactively, validating each one
and showing a preview before writing anything. Either option produces the
same kind of manifest.

> **Remember:** every device in your project must have a unique `typeShift`
> value between 0 and 31, and a unique `device` name. See
> [Concepts](CONCEPTS.md#typeshift) for details.

---

## Step 4 — Configure and build

Create a build directory and run CMake:

```bash
mkdir build && cd build
cmake ..
cmake --build . --target GenerateCommands
```

The `GenerateCommands` target runs the Python generator automatically,
reading all YAML files from `manifests/` and writing the generated C++
files into `autogen/`.

To build the full library:

```bash
cmake --build .
```

---

## Step 5 — Sync the Arduino library

Once the library is built and files are generated, sync everything to
your Arduino library folder with a single CMake target:

```bash
cmake --build . --target SyncArduinoLibrary
```

This copies:

- All library headers (`include/opx/`)
- All generated files (`autogen/`)
- `OpxDevice.cpp`, `Opx.h`, and `Opx.cpp`

to `~/Arduino/libraries/Opx/src/` (or the equivalent path on your platform).

> **Note:** Run `SyncArduinoLibrary` again any time you modify the library
> source code or regenerate files from updated manifests. `sync_arduino.py`
> (the plain-Python equivalent, no CMake required) does the same thing —
> useful for first-time setup on a machine with no CMake at all, or if
> you're developing the library's own C++ source directly.

---

## Step 6 — Write your embedded firmware

### Option A — Generate a starting point with SessionBootstrap

```bash
cd tools
python -m SessionBootstrap ../manifests ../stubs
```

It asks for a target (pick `esp32` or `avr`), which transport(s) this board
uses, and whether it should forward frames between them. It always resolves
or authors a manifest for it — every node needs its own `typeShift` and
`register()` call, regardless of role — then asks which callback stubs you
actually want generated; a purely-forwarding board can select none. The
result is written to `stubs/<NodeName>/<NodeName>.ino`, along with the exact
`cmake --build` commands to run afterward.

### Option B — Write it by hand

In your Arduino sketch:

```cpp
#include <Opx.h>

OpxDevice device;

void setup() {
    ArduinoLogger::begin(115200);

    // Register your device (generated from your manifest — sets its
    // typeShift and registers its settings/telemetry)
    registerMotorBoard(device);

    // Start serial transport
    device.beginSerial(Serial, 115200);

    // Handle incoming commands
    device.onCommand([](const Command& cmd, const uint8_t& seqNum,
                        uint8_t transportID, void* ctx) {
        if (cmd.commandType == MotorBoardCommandType::SET_SPEED) {
            // Unpack and handle the command
        }
        if (cmd.commandType == MotorBoardCommandType::STOP) {
            // Handle stop
        }
    }, nullptr);
}

void loop() {
    device.update();
}
```

Call `registerMotorBoard()` early in `setup()`, before opening any
transports — see
[Concepts: claiming your typeShift at runtime](CONCEPTS.md#claiming-your-typeshift-at-runtime)
for why this matters even for a board that's only forwarding traffic.

---

## Step 7 — Write your PC application

On the PC side, use `OpxSession` to connect to your devices. Just like an
embedded device, your PC application always needs its own manifest and a
`register()` call — even if all it does is send commands to other devices
and read their telemetry. This applies regardless of role: OmniPlexus's
architecture is fully symmetric, so every participant, PC or embedded,
needs to properly announce itself and be tracked like any other device on
the network. If your app genuinely has no commands, telemetry, or settings
of its own, give it an `identityOnly` manifest instead of skipping
`register()` — see [Concepts](CONCEPTS.md#claiming-your-typeshift-at-runtime).

Generate this with
[SessionBootstrap](CONCEPTS.md#bootstrapping-a-nodes-session-code--sessionbootstrap)
(target `pc`), or wire it up by hand:

```cpp
#include "autogen/pc/devices/PcController/PcControllerRegister.h"
#include "opx/pc/core/OpxSession.h"
#include "autogen/shared/devices/MotorBoard/MotorBoardController.h"
#include "autogen/pc/DeviceManifest.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

std::atomic<bool> running{true};
void handleSigint(int) { running = false; }

OpxSession session;

int main() {
    std::signal(SIGINT, handleSigint);

    // Claims this app's own identity — required even though everything
    // below is just controlling another device (MotorBoard), not
    // handling commands addressed to this app itself.
    registerPcController(session);

    session.connectSerial("/dev/ttyUSB0", 115200);

    // Get a typed controller for the MotorBoard
    auto& motor = session.getDevice<MotorBoardController>();
    motor.setSpeed(-200);

    session.onTelemetry([](const Telemetry& t, uint8_t transportID) {
        if (t.sourceID == TelemetrySource::MotorBoardTelemetrySource::CURRENT_SPEED) {
            // Handle telemetry
        }
    });

    // OpxSession runs its listen/processing threads internally once a
    // transport is added — there's no run() to call. This loop just
    // keeps main() alive until Ctrl+C.
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
```

If your PC app should also _receive_ its own commands (not just control
other devices), register the handler the same way you would on an
embedded device:

```cpp
session.onCommand([](const Command& cmd, uint8_t seqNum, uint8_t transportID) {
    if (cmd.commandType == PcControllerCommandType::SOME_COMMAND) {
        // Handle it
    }
});
```

Link your PC application against the OmniPlexus library in your
`CMakeLists.txt`:

```cmake
find_package(OmniPlexus REQUIRED)
target_link_libraries(my_app PRIVATE OmniPlexus::OmniPlexus)
```

---

## Step 8 — Regenerate when your manifests change

Any time you update a manifest file, regenerate and re-sync:

```bash
cd build
cmake --build . --target GenerateCommands
cmake --build . --target SyncArduinoLibrary
```

CMake watches the manifest files automatically — if nothing has changed,
`GenerateCommands` is a no-op and completes instantly.

---

## Validating your manifests

You can validate all manifests without generating any files:

```bash
cd Software
python tools/CommandGenerator/generate.py --validate-only manifests/
```

This is useful to catch errors early before committing manifest changes.

---

## Project structure overview

After setup your `Software/` folder will look like this:

```
Software/
├── manifests/               ← your YAML device manifests (gitignored except example)
├── autogen/                 ← generated C++ files (gitignored, always regenerated)
│   ├── shared/
│   │   ├── CommandTypes.h
│   │   ├── CommandPacker.h
│   │   ├── GeneratedConfig.h
│   │   ├── OpxDevices.h
│   │   └── devices/
│   │       └── MotorBoard/
│   │           └── MotorBoardController.h
│   ├── embedded/
│   │   ├── TelemetrySourceIDs.h
│   │   ├── SettingIDs.h
│   │   └── devices/
│   │       └── MotorBoard/
│   │           ├── MotorBoardRegisterAll.h
│   │           └── MotorBoardRegister.h
│   └── pc/
│       └── DeviceManifest.h
├── stubs/                   ← generated session-bootstrap starting points (gitignored)
├── include/opx/             ← library headers
├── src/                     ← library source files
├── tools/                   ← CommandGenerator, ManifestAuthoring, SessionBootstrap, Shared
├── CMakeLists.txt
├── generate_for_arduino.py
└── sync_arduino.py
```

---

## Troubleshooting

**`Could not find Boost`**
Make sure Boost is installed and CMake can find it. On Linux install
`libboost-system-dev`. On Windows set the `BOOST_ROOT` environment variable.

**`find_package(Python3) failed`**
CMake could not find Python 3. Make sure it is installed and on your PATH.
On some systems you may need `python3` instead of `python`.

**`GenerateCommands` fails with a YAML error**
Run `--validate-only` to see the specific validation error:

```bash
python tools/CommandGenerator/generate.py --validate-only manifests/
```

**`ModuleNotFoundError: No module named 'questionary'` (or `rich`)**
You only need these two if you're using ManifestAuthoring or
SessionBootstrap interactively — `GenerateCommands` itself only strictly
needs `pyyaml` and `rich` (used for its own validation output). Either way,
`pip install questionary rich` fixes it.

**Running `python -m ManifestAuthoring` or `python -m SessionBootstrap` fails
with an import error**
These must be run from inside `Software/tools/`, not the repo root —
`cd tools` first.

**Arduino sketch does not compile after sync**
Make sure you ran both `GenerateCommands` and `SyncArduinoLibrary` after
your last manifest change. Check that the Arduino IDE has refreshed its
library cache (restarting the IDE forces a refresh).

**`OpxSession` or PC headers not found**
Make sure your PC application's `CMakeLists.txt` links against
`OmniPlexus::OmniPlexus` and that `find_package(OmniPlexus)` points at
the correct build or install directory.

**Board never shows as connected / peers never see it**
Make sure `register<DeviceName>()` is actually being called — see
[Concepts: claiming your typeShift at runtime](CONCEPTS.md#claiming-your-typeshift-at-runtime).

---

## Next steps

- Read the full [Concepts](CONCEPTS.md) page for details on triggers,
  param types, and manifest rules
- See `manifests/examples.ExampleDevice.yaml` for a complete manifest with all
  supported features documented inline
- For embedded-only development without CMake, see the
  [Arduino guide](GETTING_STARTED_ARDUINO.md)
