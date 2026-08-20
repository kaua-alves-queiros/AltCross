# AltCross

A cross-platform KVM-over-network system with virtual monitors, input sharing, audio routing, and clipboard sync between multiple computers on the same local network.

![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20Android%20%7C%20iOS-blue)
![Language](https://img.shields.io/badge/core-C%20%7C%20ui-Flutter/Dart-green)

## What is AltCross?

AltCross lets you share your keyboard, mouse, screen, audio, and clipboard between multiple devices on your local network — like Synergy or a software KVM, but with virtual monitors and `Alt+Tab` system switching.

**Key features:**

- **Virtual Monitors** — create ghost displays on your main screen and switch between local and remote systems with `Alt+Tab`
- **Hot Zones** — trigger system switches by moving your cursor to screen edges/corners
- **Input Redirection** — seamless keyboard & mouse injection across devices
- **Audio Routing** — capture and play remote system audio with Opus compression
- **Clipboard Sync** — unified clipboard with history across all connected devices
- **Drag & Drop** — send files across systems by dragging them to screen edges
- **Auto Discovery** — find devices on your LAN via mDNS (zero config)
- **Wake-on-LAN** — remotely power on/off/sleep connected machines

## Architecture

```
┌────────────────────────────────────────────────┐
│              FLUTTER UI (Dart)                  │
│   Adaptive per-OS (Fluent, Cupertino, Yaru)    │
│   Screen mapping, volume panel, device config  │
└───────────────────┬────────────────────────────┘
                    │  dart:ffi
┌───────────────────▼────────────────────────────┐
│           NATIVE CORE (C)                       │
│   Virtual Display Driver                       │
│   Global Input Capture/Injection               │
│   Virtual Audio Device + Opus Codec            │
│   UDP/QUIC + WebRTC Video Streaming            │
└────────────────────────────────────────────────┘
```

- **Core (C)** — lightweight background service (~10–20 MB RAM), handles hardware/kernel access
- **UI (Flutter)** — configuration and visualization layer, communicates with Core via FFI

## Project Structure

```
AltCross/
├── AGENTS.md              # Full project spec and architecture docs
├── core/                  # Native C engine
│   ├── CMakeLists.txt
│   ├── include/altcross/  # Public headers
│   ├── src/               # Implementation
│   └── tests/             # Unit tests
└── app/                   # Flutter UI
    ├── lib/
    │   ├── main.dart
    │   ├── models/
    │   └── state/
    └── test/
```

## Building

### Core (C)

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Debug
cmake --build core/build
ctest --test-dir core/build --output-on-failure
```

### App (Flutter)

```bash
cd app
flutter analyze
flutter test
```

### Dev Build (UI + Core via FFI)

**macOS:**

```bash
./scripts/dev_macos.sh
```

**Windows:**

```bash
flutter run -d windows
```

## Supported Platforms

| Platform | Status |
|----------|--------|
| macOS    | Active development |
| Windows  | Active development |
| Linux    | Planned |
| Android  | Planned |
| iOS      | Planned |

## License

TBD
