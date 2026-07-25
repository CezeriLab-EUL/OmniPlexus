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
- **pyyaml** — the only Python dependency
- **Boost** (system component) — [boost.org](https://www.boost.org/)
- **A C++17 compiler** — GCC, Clang, or MSVC
- **Arduino IDE** (1.8.x or 2.x) or **Arduino CLI** — for flashing firmware

---

## Step 1 — Install dependencies

### pyyaml

```bash
pip install pyyaml
```

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

A fully documented example is available at
`manifests/example_device.yaml`. Use it as a reference for the correct
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

> **Remember:** every device in your project must have a unique `typeShift`
> value between 0 and 31. See [Concepts](CONCEPTS.md#typeshift) for details.

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
> source code or regenerate files from updated manifests.

---

## Step 6 — Write your embedded firmware

In your Arduino sketch:

```cpp
#include <Opx.h>

OpxDevice device;

void setup() {
    ArduinoLogger::begin(115200);

    // Register your device (generated from your manifest)
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

---

## Step 7 — Write your PC application

On the PC side, use `OpxSession` to connect to your devices:

```cpp
#include "opx/pc/core/OpxSession.h"
#include "autogen/shared/devices/MotorBoard/MotorBoardController.h"
#include "autogen/pc/DeviceManifest.h"

int main() {
    OpxSession session;

    // Add a serial transport
    session.connectSerial("/dev/ttyUSB0", 115200);

    // Get a typed controller for the MotorBoard
    auto* motor = session.getDevice<MotorBoardController>();

    // Send a command
    motor->setSpeed(-200);

    // Listen for telemetry
    session.onTelemetry([](const Telemetry& t, uint8_t transportID) {
        if (t.sourceID == TelemetrySource::MotorBoardTelemetrySource::CURRENT_SPEED) {
            // Handle telemetry
        }
    });

    session.run();
    return 0;
}
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
├── manifests/              ← your YAML device manifests (gitignored except example)
├── autogen/                ← generated C++ files (gitignored, always regenerated)
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
├── include/opx/            ← library headers
├── src/                    ← library source files
├── tools/                  ← generator tool
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

**Arduino sketch does not compile after sync**
Make sure you ran both `GenerateCommands` and `SyncArduinoLibrary` after
your last manifest change. Check that the Arduino IDE has refreshed its
library cache (restarting the IDE forces a refresh).

**`OpxSession` or PC headers not found**
Make sure your PC application's `CMakeLists.txt` links against
`OmniPlexus::OmniPlexus` and that `find_package(OmniPlexus)` points at
the correct build or install directory.

---

## Next steps

- Read the full [Concepts](CONCEPTS.md) page for details on triggers,
  param types, and manifest rules
- See `manifests/example_device.yaml` for a complete manifest with all
  supported features documented inline
- For embedded-only development without CMake, see the
  [Arduino guide](GETTING_STARTED_ARDUINO.md)
