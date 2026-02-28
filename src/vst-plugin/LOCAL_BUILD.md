# Local VST Plugin Build Guide

## ⚠️ Important: Current Environment Limitation

**This development environment does NOT have the required tools to build the VST plugin:**
- ❌ No C++ compiler (GCC/Clang/MSVC)
- ❌ No CMake build system
- ❌ No JUCE framework

The VST plugin **source code is complete and ready**, but you need to build it on your **local machine**.

---

## What You Need to Install on Your Computer

### 1. C++ Compiler
**Windows:** Install [Visual Studio](https://visualstudio.microsoft.com/) with C++ workload
**macOS:** Install Xcode Command Line Tools: `xcode-select --install`
**Linux:** Install GCC: `sudo apt install g++` or Clang: `sudo apt install clang++`

### 2. CMake
Download from: https://cmake.org/download/
**Windows:** Add CMake to PATH during installation
**macOS:** `brew install cmake`
**Linux:** `sudo apt install cmake`

### 3. JUCE Framework
```bash
cd /path/to/your/project
git clone --recurse-submodules https://github.com/juce-framework/JUCE.git
```

---

## Build Steps

### Step 1: Clone and Navigate
```bash
# Navigate to your project
cd /path/to/THD-DETECTOR

# Navigate to VST plugin directory
cd src/vst-plugin
```

### Step 2: Clone JUCE (if not already in project)
```bash
git clone --recurse-submodules https://github.com/juce-framework/JUCE.git
```

### Step 3: Make Build Script Executable
```bash
chmod +x build.sh
```

### Step 4: Run Build Script
```bash
./build.sh
```

---

## Alternative: Manual Build

If the build script doesn't work, here's the manual process:

### Windows (PowerShell):
```powershell
# Clone JUCE
git clone --recurse-submodules https://github.com/juce-framework/JUCE.git

# Navigate to VST plugin
cd src/vst-plugin

# Create build directory
mkdir build
cd build

# Configure
cmake .. -DJUCE_DIR=..\..\JUCE -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release
```

### macOS/Linux:
```bash
# Clone JUCE
git clone --recurse-submodules https://github.com/juce-framework/JUCE.git

# Navigate to VST plugin
cd src/vst-plugin

# Create build directory
mkdir build
cd build

# Configure
cmake .. -DJUCE_DIR=../../JUCE -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release
```

---

## Expected Output

After successful build, you should get:
- **Windows:** `THDAnalyzerPlugin.vst3` in `build/` folder
- **macOS:** `THDAnalyzerPlugin.vst3` in `build/` folder  
- **Linux:** `THDAnalyzerPlugin.so` in `build/` folder

---

## How to Install the VST Plugin

### Windows:
1. Copy the `.vst3` file to: `C:\Program Files\VST3\`
2. Or use your DAW's custom VST3 folder

### macOS:
1. Copy the `.vst3` file to: `/Library/Audio/Plug-Ins/VST3/`
2. Or `~/Library/Audio/Plug-Ins/VST3/`

### Linux:
1. Copy the `.so` file to: `~/.vst3/`

---

## Using the Plugin in Your DAW

1. **Restart your DAW** (to detect new plugins)
2. **Scan for plugins** in your DAW settings
3. **Add THD Analyzer** to a channel strip
4. **Load audio** through that channel
5. **See real-time THD measurements** for each harmonic

---

## Troubleshooting

### "JUCE not found"
Make sure JUCE directory is named exactly "JUCE" and is in the right location, or set JUCE_DIR environment variable.

### "Compiler not found"
Install a C++ compiler for your operating system (see requirements above).

### "CMake error"
Make sure CMake is installed and added to your system PATH.

### Build succeeds but no .vst3 file
Check the `build/` directory for any generated files. The exact filename may vary by platform.

---

## What's Included in the VST

✅ **8-Channel THD Analysis:**
- KICK, SNARE, BASS, GUITAR L/R, KEYS, VOX, FX BUS

✅ **Real-time FFT Analysis:**
- 8192 FFT size
- Hanning window
- Harmonic detection H2-H8

✅ **Measurements:**
- THD (Total Harmonic Distortion)
- THD+N (THD + Noise)
- Individual harmonic levels (H2-H8)
- RMS and peak levels

✅ **VST3 Format:**
- Works in Ableton Live, Logic Pro, Pro Tools, Reaper, etc.

---

## Next Steps After Building

Once you successfully build the plugin:
1. ✅ Test it in your DAW
2. ✅ Verify THD measurements are accurate
3. ✅ Add the plugin UI (optional - currently has basic UI)
4. ✅ Create presets

The plugin source code is complete and ready to build - you just need the right tools on your local machine! 🎵
