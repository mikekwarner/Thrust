# Thrust (BBC Micro Vector Recreation)

A native Windows C++20 recreation of the classic 1986 BBC Micro game **Thrust** by Jeremy C. Smith, built with [Raylib](https://www.raylib.com/) featuring high-definition CRT vector graphics and real-time procedural audio synthesis.

## Controls

| Key | Action |
| :--- | :--- |
| **A / D** | Rotate Ship Anti-Clockwise / Clockwise |
| **Shift** | Fire Main Thruster |
| **Space** | Activate Shield / Tractor Beam (Collect Pod & Siphon Fuel) |
| **Return / Enter** | Fire Laser Cannon |
| **P** | Pause / Resume Game |
| **F11** | Toggle Fullscreen |
| **Esc** | Return to Title Screen |
| **Q** *(on Title Screen)* | Quit Game |

## Building on Windows

### Requirements
- **CMake** (3.20 or newer)
- **C++20 Compiler** (MinGW-w64 GCC or MSVC) and **Ninja** (or Make)
- Internet connection on first configure (CMake `FetchContent` automatically downloads and builds Raylib 5.5)

### Quick Build
Double-click **`build.bat`** in File Explorer, or run from a Command Prompt:

```bat
build.bat
```

Or build manually via CMake:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The compiled executable will be located at `build\thrust.exe`.
