# Building THD Analyzer VST3 Plugin Locally

This guide explains how to build the THD Analyzer VST3 plugin on your local machine.

## Prerequisites

### All Platforms
- [CMake](https://cmake.org/download/) 3.22 or later
- [Git](https://git-scm.com/downloads)

### Platform-Specific Requirements

#### Windows
- Visual Studio 2022 (Community, Professional, or Enterprise) OR Visual Studio Build Tools 2022
- Required components:
  - MSVC v143 - VS 2022 C++ x64/x86 build tools
  - Windows 11 SDK or Windows 10 SDK
  - CMake tools for Windows (optional but recommended)

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config git
sudo apt-get install -y libgtk-3-dev libwebkit2gtk-4.0-dev
sudo apt-get install -y libasound2-dev libx11-dev libxrandr-dev
sudo apt-get install -y libxinerama-dev libxcursor-dev libfreetype6-dev
sudo apt-get install -y libfontconfig1-dev libcurl4-openssl-dev
```

#### macOS
- Xcode Command Line Tools
- CMake (via Homebrew: `brew install cmake`)

## Building on Windows

### Option 1: Using the Batch Script
```cmd
cd src\vst-plugin
build-windows.bat
```

### Option 2: Using PowerShell (Recommended)
```powershell
cd src\vst-plugin
PowerShell -ExecutionPolicy Bypass -File build-windows.ps1
```

### Option 3: Manual Build
```cmd
cd src\vst-plugin

:: Clone JUCE if not present
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git

:: Create build directory
mkdir build-windows
cd build-windows

:: Configure
cmake .. -G "Visual Studio 17 2022" -A x64

:: Build
cmake --build . --config Release --target THDAnalyzerPlugin_VST3
```

### Windows Output Location
```
src\vst-plugin\build-windows\THDAnalyzerPlugin_artefacts\VST3\THDAnalyzerPlugin.vst3
```

### Installing on Windows
Copy the VST3 bundle to your VST3 plugins folder:
```
%LOCALAPPDATA%\Programs\Common\VST3\THDAnalyzerPlugin.vst3
```

Or to a system-wide location:
```
C:\Program Files\Common Files\VST3\THDAnalyzerPlugin.vst3
```

## Building on Linux

### Option 1: Using the Shell Script
```bash
cd src/vst-plugin
./build.sh
```

### Option 2: Manual Build
```bash
cd src/vst-plugin

# Install dependencies (Ubuntu/Debian)
sudo apt-get install -y cmake g++ pkg-config
sudo apt-get install -y libgtk-3-dev libwebkit2gtk-4.0-dev
sudo apt-get install -y libasound2-dev libx11-dev libxrandr-dev
sudo apt-get install -y libxinerama-dev libxcursor-dev libfreetype6-dev
sudo apt-get install -y libfontconfig1-dev libcurl4-openssl-dev

# Clone JUCE if not present
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build . --config Release
```

### Linux Output Location
```
src/vst-plugin/build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3
```

### Installing on Linux
```bash
# User install (recommended)
mkdir -p ~/.vst3
cp -r build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3 ~/.vst3/

# Or system-wide install
sudo mkdir -p /usr/lib/vst3
sudo cp -r build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3 /usr/lib/vst3/
```

## Building on macOS

### Requirements
- macOS 10.14 or later
- Xcode Command Line Tools
- CMake

### Build Steps
```bash
cd src/vst-plugin

# Clone JUCE if not present
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git

# Create build directory
mkdir build && cd build

# Configure
cmake .. -G "Xcode"

# Build
cmake --build . --config Release
```

### macOS Output Location
```
src/vst-plugin/build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3
```

### Installing on macOS
```bash
# User install
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -r build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3 ~/Library/Audio/Plug-Ins/VST3/

# Or system-wide
sudo cp -r build/THDAnalyzerPlugin_artefacts/VST3/THDAnalyzerPlugin.vst3 /Library/Audio/Plug-Ins/VST3/
```

## Troubleshooting

### Windows Issues

#### "Visual Studio not found"
Install Visual Studio 2022 or Build Tools from:
https://visualstudio.microsoft.com/downloads/

Make sure to select "Desktop development with C++" workload.

#### "CMake not found"
Download and install CMake from https://cmake.org/download/
Make sure to check "Add CMake to the system PATH" during installation.

### Linux Issues

#### Missing webkit2gtk headers
```bash
sudo apt-get install libwebkit2gtk-4.0-dev
```

#### Missing GTK headers
```bash
sudo apt-get install libgtk-3-dev
```

### macOS Issues

#### Xcode not found
Install Xcode Command Line Tools:
```bash
xcode-select --install
```

## Plugin Information

After building, you'll find:
- **VST3 Plugin**: `THDAnalyzerPlugin.vst3` bundle
  - Windows: `Contents/x86_64-win/THDAnalyzerPlugin.dll`
  - Linux: `Contents/x86_64-linux/THDAnalyzerPlugin.so`
  - macOS: `Contents/MacOS/THDAnalyzerPlugin`

## Testing the Plugin

1. Install the VST3 plugin to your system's VST3 folder
2. Open your DAW (Ableton Live, FL Studio, Reaper, etc.)
3. Scan for new plugins
4. Insert THDAnalyzerPlugin on an audio track
5. Play audio through it to see THD analysis

## Build Configuration

The CMakeLists.txt supports these options:
- **FORMATS**: VST3 (default), can add AU (macOS), Standalone
- **BUILD_SHARED_LIBS**: OFF (static linking recommended for plugins)

To modify build settings, edit `src/vst-plugin/CMakeLists.txt`.
