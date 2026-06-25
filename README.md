# MyWinApps

A modern, high-performance, and premium native Win32 + Direct2D desktop application that serves as a unified package manager interface for Windows. It aggregates package indices and updates across **Winget**, **Scoop**, and **Chocolatey** into a single, cohesive, Fluent-style user experience.

---

## Key Features

- **Unified Index**: Search, install, upgrade, and uninstall packages across Winget, Scoop, and Chocolatey simultaneously.
- **Hardware-Accelerated UI**: Rendered entirely using native Win32, Direct2D, and DirectWrite APIs for sub-millisecond response times, low memory footprint, and high-DPI scaling support. No heavy web frames (Electron) or XAML.
- **Fluent Design & Aesthetics**: Premium glassmorphic cards, smooth indigo-violet-cyan mesh gradient background, custom title bar, and hover spotlights.
- **Non-Blocking Async Engine**: Parallel backend loads and asynchronous execution of package operations via a thread-safe task queue with dynamic concurrency controls.
- **Auto-Elevation**: Installs Chocolatey and other admin-requiring applications by triggering secure Windows UAC elevations in visible, debuggable PowerShell consoles with TLS 1.2 forced.
- **Dynamic Tool Detection**: Seamlessly detects installed package managers. Adding or installing managers at runtime refreshes the interface instantly without restarting the app.
- **Typographic Search Caret**: Positions the flashing input caret precisely by typographically measuring search string widths in real time.
- **Native Setup Installer**: Includes `Setup.ps1` (with a double-clickable `setup.bat` bypass wrapper) presenting a Fluent dark-themed setup dialog to configure folders, shortcuts, and pre-install missing package managers on clean systems.

---

## Project Structure

```
├── build/                 # Build output and executable binaries
├── src/
│   ├── core/              # Logger, ProcessRunner, and package structures
│   ├── adapters/          # Winget, Scoop, and Chocolatey adapter APIs
│   ├── services/          # Concurrency-controlled TaskQueue
│   └── gui/
│       ├── win32/         # Custom Win32 message loop, Renderer, and screens
│       └── i18n/          # English and Turkish localization dictionaries
├── CMakeLists.txt         # Project build system configuration
├── Setup.ps1              # Interactive setup dialog script
├── setup.bat              # Batch wrapper for execution policy bypass
└── README.md              # This documentation
```

---

## Getting Started

### Prerequisites

- **Windows 10 / 11**
- **Visual Studio** with C++ desktop development workload (MSVC compiler)
- **CMake** (v3.20 or newer)
- **Git**

### Building from Source

1. Clone the repository and navigate to the project directory:
   ```cmd
   git clone <repository-url>
   cd MyWinApps
   ```

2. Configure and generate build files using CMake:
   ```cmd
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   ```

3. Build the target executable in Release configuration:
   ```cmd
   cmake --build build --config Release
   ```

The compiled binary will be placed at `build/Release/MyWinApps.exe`.

---

## Installing MyWinApps

To distribute the application to a clean PC, package the following four files together:
- `MyWinApps.exe`
- `Setup.ps1`
- `Uninstall.ps1`
- `setup.bat`

Double-clicking **`setup.bat`** will launch the GUI installer as Administrator, let you choose the install location, create shortcuts, optionally download and configure Winget, Scoop, and Chocolatey automatically, and drop an **Uninstall MyWinApps** shortcut in the Start Menu.

### Uninstalling

Open the **Start Menu → MyWinApps → Uninstall MyWinApps** shortcut, or run `Uninstall.ps1` directly as Administrator. The script removes the install folder, both shortcuts, and any crash logs / dumps left behind by the crash handler. Package managers (Winget, Scoop, Chocolatey) are intentionally left installed.

---

## License

This project is licensed under the MIT License - see the LICENSE file for details.
