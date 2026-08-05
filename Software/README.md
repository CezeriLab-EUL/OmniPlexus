# OmniPlexus

A modular, cross-platform C++ middleware library for robotics. OmniPlexus
handles the communication layer between a PC and one or more microcontrollers
so you can focus on what your robot does rather than how its parts talk to
each other.

**Supported targets:** ESP32 · Arduino (AVR) · STM32 · CH32 · PC (Linux / macOS / Windows)

**Supported transports:** WiFi (TCP) · Serial · HTTP · CDnC (custom parallel bus)

---

## Where to start

### I want to understand how OmniPlexus works

→ [Concepts (Start Here)](docs/CONCEPTS.md)

### I want to flash a microcontroller (no PC application)

→ [Getting Started: Arduino](docs/GETTING_STARTED_ARDUINO.md)

### I want to build both a PC application and embedded firmware

→ [Getting Started: CMake](docs/GETTING_STARTED_CMAKE.md)

---

## How it works in one paragraph

You describe each microcontroller module in your robot using a simple YAML
file called a **device manifest** — listing the commands it accepts, the
telemetry it produces, and the settings it exposes. You can write these by
hand, or author them interactively with the **ManifestAuthoring** tool. The
OmniPlexus generator reads your manifests and produces C++ glue code
automatically. On the microcontroller side you include the generated files
and call `OpxDevice` to handle communication. On the PC side you use
`OpxSession` and generated controller classes to send commands and receive
telemetry over WiFi, Serial, or any other supported transport. If you'd
rather not hand-write your setup code either, **SessionBootstrap** generates
a starting-point sketch or PC application for you interactively.

---

## Repository structure

```
Software/
├── manifests/               ← your YAML device manifests
├── autogen/                 ← generated C++ files (not committed to git)
├── stubs/                   ← generated session-bootstrap starting points (not committed to git)
├── include/opx/             ← library headers
│   ├── shared/              ← compiles on all targets
│   ├── embedded/            ← microcontroller targets only
│   └── pc/                  ← PC targets only
├── src/                     ← library source files
├── tools/
│   ├── CommandGenerator/    ← turns YAML manifests into C++ glue code
│   ├── ManifestAuthoring/   ← interactive wizard for writing manifests
│   ├── SessionBootstrap/    ← interactive wizard for session-bootstrap code
│   ├── Shared/              ← code shared by the three tools above
│   └── requirements.txt     ← Python dependencies for the tools
├── docs/                    ← guides and reference
├── CMakeLists.txt
├── generate_for_arduino.py  ← zero-config generator for Arduino users
└── sync_arduino.py          ← sync library files to Arduino — first-time
                                setup without CMake, or developing the
                                library's own C++ source
```

---

## Quick start

```bash
# Clone the repo
git clone https://github.com/CezeriLab-EUL/OmniPlexus.git
cd OmniPlexus/Software

# Install the tools' Python dependencies
pip install -r tools/requirements.txt

# Author a manifest interactively...
cd tools && python -m ManifestAuthoring ../manifests && cd ..
# ...or write your own YAML by hand in manifests/

# Then generate:
python generate_for_arduino.py --manifests-folder manifests/
```

---

## License

MIT License — see [LICENSE](../LICENSE-SOFTWARE) for details.
