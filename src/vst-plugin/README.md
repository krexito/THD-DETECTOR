# THD Analyzer VST Plugin

A real-time THD (Total Harmonic Distortion) analyzer plugin built with JUCE framework.

## Project Status

**⚠️ VST Plugin Development Started**

This is the beginning of creating a real VST plugin. The plugin code has been written, but you need to set up the JUCE framework and build system.

## What's Been Created

### Core Plugin Files
- **THDAnalyzerPlugin.h** - Plugin header with FFT analyzer and channel classes
- **THDAnalyzerPlugin.cpp** - Main plugin processor implementation
- **CMakeLists.txt** - Build configuration for JUCE

### Features Implemented
- ✅ 8-channel THD analysis (KICK, SNARE, BASS, GTR L/R, KEYS, VOX, FX BUS)
- ✅ Real FFT analysis using JUCE DSP module (8192 FFT size)
- ✅ THD and THD+N measurement
- ✅ Harmonic analysis H2-H8
- ✅ Level metering (RMS + peak)
- ✅ Channel data structure with mute/solo support

## Next Steps to Build the Plugin

### Option 1: Install JUCE and Build

1. **Clone JUCE framework:**
   ```bash
   git clone --recurse-submodules https://github.com/juce-framework/JUCE.git
   ```

2. **Set up environment:**
   - Install CMake
   - Install a C++ compiler (GCC/Clang/MSVC)
   - Install Projucer (comes with JUCE)

3. **Build with Projucer:**
   - Open the `.jucer` file (need to create this from CMakeLists.txt)
   - Generate project files for your IDE
   - Build the VST3 plugin

### Option 2: Use CMake directly

```bash
# Create build directory
mkdir build
cd build

# Configure with JUCE
cmake .. -DJUCE_DIR=/path/to/JUCE

# Build
cmake --build . --config Release
```

### Option 3: Use IDE integration

1. Open in Visual Studio, Xcode, or VS Code
2. Configure JUCE paths
3. Build the VST3 target

## Plugin Architecture

### Audio Processing
- **THDAnalyzerPlugin** - Main audio processor
- **FFTAnalyzer** - Real-time FFT analysis class
- **ChannelData** - Per-channel measurement data

### Signal Flow
```
Input Audio → Buffer → FFT Analysis → THD Calculation → Channel Data
```

### Key Classes
- `FFTAnalyzer::analyze()` - Performs FFT and calculates THD/THD+N
- `ChannelData` - Stores measurements per channel
- `processBlock()` - Main audio processing loop

## Requirements

- **JUCE Framework** (6.0 or later)
- **C++17** compatible compiler
- **CMake** 3.15 or later
- **Platform**: Windows, macOS, or Linux

## Current Limitations

- **Not yet built** - Need to set up JUCE build system
- **No UI yet** - Basic processor created, need to add editor
- **No VST2 support** - Only VST3 configured

## Future Enhancements

- [ ] Create proper Projucer project file
- [ ] Add custom UI with channel meters
- [ ] Add master brain analyzer view
- [ ] Support VST2 and AU formats
- [ ] Add configuration options
- [ ] Optimize FFT performance

## Integration with Web Version

This VST plugin contains the same FFT analysis logic as the web version:
- Same Hanning window function
- Same harmonic detection (H2-H8)
- Same THD/THD+N calculation
- Same frequency range (20Hz - 2kHz for fundamental)

The main difference is:
- **Web version**: Uses Web Audio API AnalyserNode
- **VST version**: Uses JUCE DSP FFT module
- **VST version**: Runs in real-time within DAW

## Troubleshooting

### Missing JUCE
If you get errors about JUCE not found, you need to either:
1. Install JUCE system-wide
2. Clone JUCE as a submodule
3. Update CMakeLists.txt with correct path

### Build Errors
- Make sure you have C++17 or later
- Verify all JUCE modules are available
- Check that your IDE supports CMake projects

## Contributing

This plugin is part of the THD Analyzer project. Feel free to:
- Submit issues
- Create pull requests
- Suggest improvements

## License

Open source - See project root for details
