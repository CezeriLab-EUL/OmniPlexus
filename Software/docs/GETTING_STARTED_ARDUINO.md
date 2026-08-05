# Getting Started — Arduino (Embedded Only)

This guide is for users who want to use OmniPlexus on a microcontroller
(ESP32, Arduino Uno/Nano, STM32, CH32) without setting up CMake.

If you are also building a PC-side application, see the
[CMake guide](GETTING_STARTED_CMAKE.md) instead.

Before continuing, read [OmniPlexus Concepts](CONCEPTS.md) to understand
how the library works.

---

## What you need

- **Python 3** — [python.org/downloads](https://www.python.org/downloads/)
- **pyyaml**, **questionary**, **rich** — Python dependencies for the tools
- **Arduino IDE** (1.8.x or 2.x) or **Arduino CLI**

---

## Step 1 — Install Python dependencies

Open a terminal and run:

```bash
pip install pyyaml questionary rich
```

(Once you've cloned the repo in [Step 3](#step-3--clone-the-repo-for-the-generator),
you can instead run `pip install -r tools/requirements.txt` from `Software/`
— same three packages, listed there for convenience.)

---

## Step 2 — Get the OmniPlexus library

### Option A — Download a release (recommended)

1. Go to the [OmniPlexus Releases page](https://github.com/CezeriLab-EUL/OmniPlexus/releases)
2. Download the latest `Opx-arduino.zip`
3. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library...**
4. Select the downloaded ZIP

The library is now installed. Skip to [Step 3](#step-3--clone-the-repo-for-the-generator).

### Option B — Install manually

If you prefer to install by hand:

1. Download and extract the ZIP
2. Copy the extracted folder to your Arduino libraries directory:

| Platform | Path                                 |
| -------- | ------------------------------------ |
| Linux    | `~/Arduino/libraries/Opx/`           |
| macOS    | `~/Documents/Arduino/libraries/Opx/` |
| Windows  | `Documents\Arduino\libraries\Opx\`   |

---

## Step 3 — Clone the repo for the generator

The generator and its companion tools live in the OmniPlexus repository. You
only need to clone it once — you don't need to build anything from it.

```bash
git clone https://github.com/CezeriLab-EUL/OmniPlexus.git
cd OmniPlexus/Software
```

---

## Step 4 — Write your device manifests

Create a `manifests/` folder in your project directory and add a YAML file
for each microcontroller in your robot.

```
my_robot/
└── manifests/
    ├── MotorBoard.yaml
    └── SensorBoard.yaml
```

### Option A — Write YAML by hand

A fully documented example is available at
`OmniPlexus/Software/manifests/example_device.yaml`. Use it as a reference
for the correct structure and all available options.

Here is a minimal example to get started:

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

From `OmniPlexus/Software/tools/`:

```bash
cd tools
python -m ManifestAuthoring /path/to/my_robot/manifests
```

This walks you through the same fields interactively, validating each one
and showing a preview before writing anything. Either option produces the
same kind of manifest — the generator doesn't care which one you used.

> **Remember:** every device in your project must have a unique `typeShift`
> value between 0 and 31, and a unique `device` name. See
> [Concepts](CONCEPTS.md#typeshift) for details.

---

## Step 5 — Generate your device files

Navigate to the OmniPlexus repo root and run the generator, pointing it at
your project's manifests folder:

```bash
cd OmniPlexus/Software
python generate_for_arduino.py --manifests-folder /path/to/my_robot/manifests
```

The script auto-detects your Arduino library path and writes the generated
files directly into `~/Arduino/libraries/Opx/src/autogen/` (or the equivalent
path on your platform).

If you want to preview what will be generated without writing any files:

```bash
python generate_for_arduino.py --manifests-folder /path/to/my_robot/manifests --dry-run
```

If you have a non-standard Arduino library path:

```bash
python generate_for_arduino.py \
  --manifests-folder /path/to/my_robot/manifests \
  --library-path /custom/path/to/Arduino/libraries/Opx/src
```

To find your Arduino library path, open the Arduino IDE and go to
**File → Preferences → Sketchbook location** — your libraries folder is
`<sketchbook>/libraries/`.

---

## Step 6 — Write your sketch

### Option A — Generate a starting point with SessionBootstrap

From `OmniPlexus/Software/tools/`:

```bash
cd tools
python -m SessionBootstrap /path/to/my_robot/manifests /path/to/my_robot/stubs
```

It asks for a target (pick `esp32` or `avr`), which transport(s) this board
uses (WiFi/Serial/HTTP for ESP32, Serial only for AVR), and whether it
should forward frames between them. It always links to the manifest you
wrote in Step 4 (or launches ManifestAuthoring on the spot if you skip
straight here) — every node needs its own `typeShift` and `register()` call,
regardless of role — then asks which callback stubs (`onCommand`,
`onTelemetry`, and so on) you actually want generated; a purely-forwarding
board can select none. It writes a real, compilable starting sketch to
`my_robot/stubs/<NodeName>/<NodeName>.ino` — open that folder in the Arduino
IDE and build on it from there. It'll also print the exact regenerate
command you need any time your manifest changes.

### Option B — Write it by hand

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

The `registerMotorBoard()` function and `MotorBoardCommandType` constants are
generated automatically from your `MotorBoard.yaml` manifest. Call
`registerMotorBoard()` early in `setup()`, before opening any transports —
see [Concepts: claiming your typeShift at runtime](CONCEPTS.md#claiming-your-typeshift-at-runtime)
for why this matters even for a board that's only forwarding traffic.

---

## Step 7 — Regenerate when your manifests change

Any time you add, remove, or change a command, telemetry source, or setting
in your YAML files, just re-run the generator:

```bash
python generate_for_arduino.py --manifests-folder /path/to/my_robot/manifests
```

The Arduino IDE picks up the new files automatically on the next compile.

---

## Validating your manifests without generating

You can check your manifests for errors before generating:

```bash
python generate_for_arduino.py --manifests-folder /path/to/my_robot/manifests --validate-only
```

This is useful to catch typos, duplicate IDs, duplicate names, or missing
fields early.

---

## Troubleshooting

**`pip: command not found`**
Try `pip3` instead of `pip`. On Windows, try `python -m pip install pyyaml questionary rich`.

**`ModuleNotFoundError: No module named 'questionary'` (or `rich`)**
You only need these two if you're using ManifestAuthoring or SessionBootstrap
interactively — `generate_for_arduino.py`/`sync_arduino.py` on their own
only need `pyyaml`. Either way, `pip install questionary rich` fixes it.

**Running `python -m ManifestAuthoring` or `python -m SessionBootstrap` fails
with an import error**
These must be run from inside `Software/tools/`, not the repo root —
`cd tools` first.

**`Error: Could not detect Arduino library path`**
Your Arduino sketchbook is in a non-standard location. Use `--library-path`
to point the script at the correct folder.

**`Error: Manifests folder not found`**
Make sure the path you pass to `--manifests-folder` exists and contains
at least one `.yaml` file.

**`unknown type name 'OpxDevice'`** in Arduino IDE
This usually means the library was not installed correctly. Verify that
`~/Arduino/libraries/Opx/src/Opx.h` exists.

**Compilation errors after regenerating**
Make sure you regenerated after every manifest change. If errors persist,
run `--validate-only` to check for manifest issues before generating.

**Board never shows as connected / peers never see it**
Make sure `register<DeviceName>()` is actually being called in `setup()` —
see [Concepts: claiming your typeShift at runtime](CONCEPTS.md#claiming-your-typeshift-at-runtime).

---

## Next steps

- Read the full [Concepts](CONCEPTS.md) page for details on triggers,
  param types, and manifest rules
- See `manifests/example_device.yaml` for a complete manifest with all
  supported features documented inline
- For PC-side development, see the [CMake guide](GETTING_STARTED_CMAKE.md)
