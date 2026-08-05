# OmniPlexus Concepts

This page explains the core ideas behind OmniPlexus. Both the
[Arduino guide](GETTING_STARTED_ARDUINO.md) and the
[CMake guide](GETTING_STARTED_CMAKE.md) assume you have read this first.

---

## What is OmniPlexus?

OmniPlexus is a C++ middleware library for robotics. It handles the
communication layer between a PC (or high-level controller) and one or more
microcontrollers — so you can focus on what your robot does rather than how
its parts talk to each other.

It works across multiple transports (WiFi, Serial, a custom parallel bus called
CDnC) and multiple microcontroller families (ESP32, AVR/Arduino, STM32, CH32).

---

## How it works — the big picture

```
┌─────────────────────┐         WiFi / Serial / CDnC        ┌──────────────────────┐
│   PC / Operator     │ ◄──────────────────────────────────► │   Microcontroller    │
│   (OpxSession)      │                                       │   (OpxDevice)        │
└─────────────────────┘                                       └──────────────────────┘
         │                                                              │
         │  sends Commands                                   receives Commands
         │  receives Telemetry                               sends Telemetry
         │  reads/writes Settings                            reads/writes Settings
```

- **Commands** — instructions sent from PC to microcontroller (e.g. `MOVE`, `LED_SET`)
- **Telemetry** — data streamed from microcontroller to PC (e.g. temperature, sensor readings)
- **Settings** — persistent configuration values that can be read or written from either side

---

## Device Manifests

A **device manifest** is a YAML file that describes one microcontroller module
in your robot — what commands it accepts, what telemetry it produces, and what
settings it exposes.

You write one manifest per physical device. The generator reads all your
manifests and produces the C++ glue code automatically.

### Example manifest structure

```yaml
version: "1.0"
description: A brief description of this device
device: MyDevice # PascalCase, alphanumeric only
typeShift: 1 # unique integer 0-31 per device in your project

commands:
  - id: 1
    name: DO_SOMETHING # UPPER_SNAKE_CASE
    description: Does something useful
    acknowledges: false # true = PC waits for confirmation
    params:
      - name: value
        type: UINT8
        required: true
        description: A value between 0 and 255

telemetry:
  - id: 1
    name: SENSOR_READING
    description: Current sensor value
    type: INT16
    trigger:
      type: periodic
      intervalMs: 500 # send every 500ms

settings:
  - id: 1
    name: SAMPLE_RATE_MS
    description: How often to sample in milliseconds
    type: UINT16
```

A complete working example is available in `manifests/ExampleDevice.yaml`.

---

## Writing manifests interactively — ManifestAuthoring

Instead of hand-writing YAML, you can run the **ManifestAuthoring** tool from
`Software/tools/`:

```bash
cd tools
python -m ManifestAuthoring ../manifests
```

It walks you through a device's name, `typeShift`, target, commands,
telemetry, and settings one at a time — validating each field as you go and
showing you a preview before writing anything to disk. The manifest it
produces is exactly the same shape as one you'd write by hand; the generator
doesn't know or care which way a manifest was created.

---

## Bootstrapping a node's session code — SessionBootstrap

Every node — a microcontroller or a PC application — needs some setup code:
which transport(s) to open, whether it forwards traffic between them, and
its own identity (every node needs one, regardless of role — see
[Concepts: claiming your typeShift at runtime](#claiming-your-typeshift-at-runtime)
below). Writing this by hand is repetitive and easy to get subtly wrong, so
**SessionBootstrap** generates a starting point for you:

```bash
cd tools
python -m SessionBootstrap ../manifests ../stubs
```

It asks for a target (`esp32`, `avr`, or `pc`) and the transport(s) that node
uses, whether it should forward frames between them, and always resolves or
authors a manifest for it — calling into ManifestAuthoring directly if none
exists yet — since every node needs a `typeShift` and a `register()` call
regardless of whether it also handles application-level commands. The only
thing that's actually optional is _which_ callback stubs (`onCommand`,
`onTelemetry`, and so on) get generated — a purely-forwarding node can select
none of them and still get its identity properly claimed. The result is
written to `stubs/<NodeName>/<NodeName>.ino` (or `.cpp` for a `pc` target) —
a real, compilable starting point you're expected to edit further, not a
finished program.

**This applies equally to PC applications that only control other devices.**
Even a PC app that just sends commands and reads telemetry — never handling
commands addressed to itself — still needs its own manifest and `register()`
call. OmniPlexus's architecture is fully symmetric: any node can be a master
or slave to any other, so every participant needs to properly announce
itself and be tracked like any other device on the network. See
[Step 7 of the CMake guide](GETTING_STARTED_CMAKE.md) for what this looks
like in practice.

---

## typeShift

Every device in your project must have a unique `typeShift` value between 0
and 31. This is how OmniPlexus tells devices apart on the wire — it namespaces
all command and telemetry IDs so they never collide even when multiple devices
are connected at the same time.

If two devices share the same `typeShift`, the generator will catch this and
report an error before producing any files.

### Claiming your typeShift at runtime

Reserving a `typeShift` in a manifest is only half the picture — the running
device also has to _claim_ it. The generator produces a
`register<DeviceName>()` function (in `<DeviceName>Register.h`) that does
this for you: it calls `setTypeShift()` and, unless the manifest is
`identityOnly`, also registers that device's settings and telemetry. Call it
once, early in `setup()` (embedded) or at the start of `main()` (PC), before
opening any transports:

```cpp
registerMotorBoard(device);   // embedded (OpxDevice&)
registerMotorBoard(session);  // PC (OpxSession&)
```

This should be called for every node that has a manifest — regardless of
whether that node also forwards traffic for other devices. Without it, the
node never announces itself and peers never see it as connected, even if
everything else is wired up correctly.

---

## Param types

| Type     | C++ equivalent | Size     |
| -------- | -------------- | -------- |
| `UINT8`  | `uint8_t`      | 1 byte   |
| `INT8`   | `int8_t`       | 1 byte   |
| `UINT16` | `uint16_t`     | 2 bytes  |
| `INT16`  | `int16_t`      | 2 bytes  |
| `UINT32` | `uint32_t`     | 4 bytes  |
| `INT32`  | `int32_t`      | 4 bytes  |
| `FLOAT`  | `float`        | 4 bytes  |
| `STRING` | `const char*`  | variable |

---

## Telemetry triggers

| Trigger     | When it fires                                   |
| ----------- | ----------------------------------------------- |
| `onChange`  | When the value changes by more than `threshold` |
| `periodic`  | At a fixed interval specified by `intervalMs`   |
| `onRequest` | Only when explicitly requested by the PC        |

---

## What the generator produces

From your manifest files, the generator produces:

| File                    | Location                             | Used by  |
| ----------------------- | ------------------------------------ | -------- |
| `CommandTypes.h`        | `autogen/shared/`                    | Both     |
| `CommandPacker.h`       | `autogen/shared/`                    | Both     |
| `GeneratedConfig.h`     | `autogen/shared/`                    | Both     |
| `OpxDevices.h`          | `autogen/shared/`                    | Embedded |
| `<Device>Controller.h`  | `autogen/shared/devices/<Device>/`   | Both     |
| `TelemetrySourceIDs.h`  | `autogen/embedded/`                  | Embedded |
| `SettingIDs.h`          | `autogen/embedded/`                  | Embedded |
| `<Device>RegisterAll.h` | `autogen/embedded/devices/<Device>/` | Embedded |
| `<Device>Register.h`    | `autogen/embedded/devices/<Device>/` | Embedded |
| `DeviceManifest.h`      | `autogen/pc/`                        | PC       |
| `CommandRegistry.cpp`   | `src/autogen/pc/`                    | PC       |

You never edit these files — they are always regenerated from your manifests.

---

## Manifest rules at a glance

- `device` — PascalCase, alphanumeric only (e.g. `MotorBoard`, not `motor_board`)
  — and must be unique across every manifest in your project
- `typeShift` — unique integer 0–31 across all devices in your project
- Command, telemetry, and setting `name` fields — `UPPER_SNAKE_CASE`
- Command and telemetry `id` values — start from 1, unique within each section
- Param `name` fields must be unique **within their own command** (two
  different commands may each have a param with the same name)
- Maximum 3 parameters per command (strongly recommended for embedded targets)
- Optional parameters must come last and must have a `default` value
- Only one optional parameter per command is allowed
- `STRING` params must specify `maxLength`, up to a maximum of **16** —
  `ValueSource`'s underlying storage is a fixed 16 bytes, so a longer value
  can't actually be carried

---

## Next steps

- **Arduino / embedded only** → [Getting Started: Arduino](GETTING_STARTED_ARDUINO.md)
- **PC + embedded with CMake** → [Getting Started: CMake](GETTING_STARTED_CMAKE.md)
