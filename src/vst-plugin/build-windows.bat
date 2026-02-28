@echo off
setlocal EnableDelayedExpansion

echo ============================================
echo THD Analyzer VST3 Plugin - Windows Build
echo ============================================
echo.

:: Check if we're running on Windows
if not "%OS%"=="Windows_NT" (
    echo ERROR: This script is intended for Windows only.
    exit /b 1
)

:: Configuration
set "BUILD_DIR=build-windows"
set "VST3_DIR=%BUILD_DIR%\THDAnalyzerPlugin_artefacts\VST3"
set "CMAKE_GENERATOR=Visual Studio 17 2022"
set "CMAKE_ARCH=x64"

:: Find Visual Studio
echo [1/6] Looking for Visual Studio installation...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio or Build Tools not found.
    echo Please install Visual Studio 2022 or Visual Studio Build Tools:
    echo https://visualstudio.microsoft.com/downloads/
    echo.
    echo Required components:
    echo   - MSVC v143 - VS 2022 C++ x64/x86 build tools
    echo   - Windows 11 SDK or Windows 10 SDK
    echo   - CMake tools for Windows
    exit /b 1
)

:: Get the installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "%VS_PATH%" (
    echo ERROR: Could not find Visual Studio installation path.
    exit /b 1
)

echo Found Visual Studio at: %VS_PATH%

:: Check for CMake
echo [2/6] Checking for CMake...
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH.
    echo Please install CMake from https://cmake.org/download/
    echo Make sure to add CMake to the system PATH during installation.
    exit /b 1
)
for /f "tokens=*" %%i in ('cmake --version ^| findstr /C:"cmake version"') do (
    echo Found: %%i
)

:: Check for Git
echo [3/6] Checking for Git...
where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: Git not found in PATH.
    echo Please install Git from https://git-scm.com/download/win
    exit /b 1
)
echo Found: Git

:: Set up Visual Studio environment
echo [4/6] Setting up Visual Studio build environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to set up Visual Studio environment.
    echo Make sure Visual Studio C++ build tools are installed.
    exit /b 1
)
echo Environment configured for x64 build.

:: Clone JUCE if not present
echo [5/6] Checking for JUCE framework...
if not exist "JUCE\CMakeLists.txt" (
    echo JUCE not found. Cloning from GitHub...
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
    if errorlevel 1 (
        echo ERROR: Failed to clone JUCE framework.
        exit /b 1
    )
    echo JUCE cloned successfully.
) else (
    echo JUCE already present.
)

:: Create build directory
echo [6/6] Configuring and building VST3 plugin...
if exist "%BUILD_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

:: Configure with CMake
echo.
echo Configuring with CMake...
echo Generator: %CMAKE_GENERATOR%
echo Architecture: %CMAKE_ARCH%
echo.

cmake .. -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    cd ..
    exit /b 1
)

:: Build the VST3 plugin
echo.
echo Building VST3 plugin... This may take several minutes.
echo.

cmake --build . --config Release --target THDAnalyzerPlugin_VST3 -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo ERROR: Build failed.
    cd ..
    exit /b 1
)

:: Return to source directory
cd ..

:: Check if the plugin was created
echo.
echo ============================================
echo Build Complete!
echo ============================================
echo.

set "PLUGIN_PATH=%VST3_DIR%\THDAnalyzerPlugin.vst3"
if exist "%PLUGIN_PATH%" (
    echo VST3 Plugin created successfully:
    echo   %CD%\%PLUGIN_PATH%
    echo.
    echo The plugin bundle contains:
    dir "%PLUGIN_PATH%" /s /b 2>nul | findstr /C:"Contents"
    echo.
    echo To install the plugin:
    echo   1. Copy THDAnalyzerPlugin.vst3 to your VST3 plugins folder:
    echo      %%LOCALAPPDATA%%\Programs\Common\VST3\
    echo   2. Or copy to a custom VST3 path and add it to your DAW
    echo.
    echo Plugin details:
    for %%I in ("%PLUGIN_PATH%\Contents\x86_64-win\*.dll") do (
        echo   File: %%~nxI
        echo   Size: %%~zI bytes
    )
) else (
    echo WARNING: Plugin bundle not found at expected location.
    echo Searching for any .vst3 files in build directory...
    for /r "%BUILD_DIR%" %%f in (*.vst3) do (
        echo Found: %%f
    )
)

echo.
echo ============================================
echo Build process completed.
echo ============================================

exit /b 0
