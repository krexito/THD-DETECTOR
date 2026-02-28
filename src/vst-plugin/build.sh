#!/bin/bash

# THD Analyzer VST Plugin Build Script
# This script builds the VST3 plugin using JUCE framework

echo "=== THD Analyzer VST Plugin Build ==="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Run this from the vst-plugin directory."
    exit 1
fi

# Check for required tools
echo "Checking build requirements..."

if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Install CMake 3.15 or later."
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "Error: C++ compiler not found. Install GCC or Clang."
    exit 1
fi

# Check for JUCE
if [ ! -d "$JUCE_DIR" ] && [ ! -d "../JUCE" ] && [ ! -d "../../JUCE" ]; then
    echo "Warning: JUCE directory not found."
    echo "Please set JUCE_DIR environment variable or clone JUCE to this directory."
    echo ""
    echo "To clone JUCE:"
    echo "  git clone --recurse-submodules https://github.com/juce-framework/JUCE.git"
    exit 1
fi

# Determine JUCE path
if [ -n "$JUCE_DIR" ]; then
    JUCE_PATH="$JUCE_DIR"
elif [ -d "../JUCE" ]; then
    JUCE_PATH="../JUCE"
elif [ -d "../../JUCE" ]; then
    JUCE_PATH="../../JUCE"
fi

echo "Found JUCE at: $JUCE_PATH"

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build with CMake..."
cmake .. -DJUCE_DIR="$JUCE_PATH" -DCMAKE_BUILD_TYPE=Release

# Build the plugin
echo "Building VST3 plugin..."
cmake --build . --config Release

# Check if build was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "=== Build Successful! ==="
    echo "VST3 plugin should be in: build/THDAnalyzerPlugin.vst3"
    
    # Find the built plugin
    if [ -d "THDAnalyzerPlugin.vst3" ]; then
        echo "Plugin found: $(pwd)/THDAnalyzerPlugin.vst3"
    else
        echo "Looking for plugin files..."
        find . -name "*.vst3" -o -name "*.vst3" 2>/dev/null
    fi
else
    echo ""
    echo "=== Build Failed ==="
    echo "Check the error messages above."
    exit 1
fi
