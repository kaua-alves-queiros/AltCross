# AltCross

Software KVM for sharing keyboard, mouse, display, audio, and clipboard across multiple machines on a local network.

## Features

- **Virtual monitors** — create software-driven displays and switch between local/remote systems with `Alt+Tab`
- **Hot zones** — cursor hits a screen edge or corner to trigger system switch
- **Input redirection** — global keyboard and mouse injection via OS-native APIs (CGEvent on macOS, SendInput on Windows, uinput on Linux)
- **Audio routing** — cross-machine audio capture and playback with Opus codec over UDP
- **Clipboard sync** — text, images, and file transfers between connected machines with history
- **Drag and drop** — drag files to screen edges to send them to another system
- **Device discovery** — automatic LAN detection via mDNS broadcast
- **Wake-on-LAN** — remote power management (wake, sleep, restart, shutdown)

## Architecture

```
┌──────────────────────────────────────────┐
│           Flutter UI (Dart)              │
│  Adaptive per-OS (Fluent/Cupertino/Yaru)│
│  Screen mapping, volume, device config   │
└──────────────────┬───────────────────────┘
                   │ dart:ffi
┌──────────────────▼───────────────────────┐
│          Native Core (C)                 │
│  Virtual display driver (IDD/CGDisplay)  │
│  Global input capture/injection          │
│  Virtual audio device + Opus codec       │
│  UDP/QUIC + WebRTC video streaming       │
└──────────────────────────────────────────┘
```

The core runs as a lightweight background daemon (~10-20 MB RAM) with direct hardware/kernel access. The Flutter UI communicates with it through FFI bindings.

## Requirements

- **Core:** CMake 3.16+, C99 compiler (GCC/Clang/MSVC)
- **App:** Flutter 3.x, Dart 3.x

## Building

### Core

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Debug
cmake --build core/build
ctest --test-dir core/build --output-on-failure
```

### Flutter App

```bash
cd app
flutter pub get
flutter analyze
flutter test
```

### Full dev build (macOS)

```bash
./scripts/dev_macos.sh
```

### Full dev build (Windows)

```bash
flutter run -d windows
```

## Project Layout

```
AltCross/
├── core/                   # Native C engine
│   ├── include/altcross/   # Public headers
│   ├── src/                # Implementation
│   │   └── platform/       # OS-specific code (CGEvent, SendInput, uinput)
│   ├── tests/              # Unit tests
│   └── tools/              # Daemon entry point
├── app/                    # Flutter UI
│   ├── lib/
│   │   ├── models/         # Data models
│   │   └── state/          # State management
│   └── test/
└── scripts/                # Build/dev scripts
```

## Platform Status

| Platform | Status |
|----------|--------|
| macOS    | Development |
| Windows  | Development |
| Linux    | Planned |
| Android  | Planned |
| iOS      | Planned |

## License

MIT License. See [LICENSE](LICENSE) for details.
