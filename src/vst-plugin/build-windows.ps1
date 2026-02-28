# THD Analyzer VST3 Plugin - Windows Build Script
# Run with: PowerShell -ExecutionPolicy Bypass -File build-windows.ps1

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "THD Analyzer VST3 Plugin - Windows Build" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$BuildDir = "build-windows"
$Vst3Dir = "$BuildDir\THDAnalyzerPlugin_artefacts\VST3"
$CMakeGenerator = "Visual Studio 17 2022"
$CMakeArch = "x64"

# Check if running on Windows
if ($env:OS -ne "Windows_NT") {
    Write-Error "This script is intended for Windows only."
    exit 1
}

# Step 1: Find Visual Studio
Write-Host "[1/6] Looking for Visual Studio installation..." -ForegroundColor Yellow
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $VsWhere)) {
    Write-Host "ERROR: Visual Studio or Build Tools not found." -ForegroundColor Red
    Write-Host "Please install Visual Studio 2022 or Visual Studio Build Tools:"
    Write-Host "https://visualstudio.microsoft.com/downloads/"
    Write-Host ""
    Write-Host "Required components:"
    Write-Host "  - MSVC v143 - VS 2022 C++ x64/x86 build tools"
    Write-Host "  - Windows 11 SDK or Windows 10 SDK"
    Write-Host "  - CMake tools for Windows"
    exit 1
}

$VsPath = & $VsWhere -latest -property installationPath
if (-not $VsPath -or -not (Test-Path $VsPath)) {
    Write-Error "Could not find Visual Studio installation path."
    exit 1
}

Write-Host "Found Visual Studio at: $VsPath" -ForegroundColor Green

# Step 2: Check for CMake
Write-Host "[2/6] Checking for CMake..." -ForegroundColor Yellow
try {
    $CMakeVersion = cmake --version | Select-String "cmake version" | Select-Object -First 1
    if (-not $CMakeVersion) {
        throw "CMake not found"
    }
    Write-Host "Found: $CMakeVersion" -ForegroundColor Green
} catch {
    Write-Host "ERROR: CMake not found in PATH." -ForegroundColor Red
    Write-Host "Please install CMake from https://cmake.org/download/"
    Write-Host "Make sure to add CMake to the system PATH during installation."
    exit 1
}

# Step 3: Check for Git
Write-Host "[3/6] Checking for Git..." -ForegroundColor Yellow
try {
    $null = Get-Command git -ErrorAction Stop
    Write-Host "Found: Git" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Git not found in PATH." -ForegroundColor Red
    Write-Host "Please install Git from https://git-scm.com/download/win"
    exit 1
}

# Step 4: Set up Visual Studio environment
Write-Host "[4/6] Setting up Visual Studio build environment..." -ForegroundColor Yellow
$VcVarsBat = "$VsPath\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $VcVarsBat)) {
    Write-Error "Failed to find vcvars64.bat. Make sure Visual Studio C++ build tools are installed."
    exit 1
}

# Import Visual Studio environment
& cmd /c "`"$VcVarsBat`" && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}
Write-Host "Environment configured for x64 build." -ForegroundColor Green

# Step 5: Clone JUCE if not present
Write-Host "[5/6] Checking for JUCE framework..." -ForegroundColor Yellow
if (-not (Test-Path "JUCE\CMakeLists.txt")) {
    Write-Host "JUCE not found. Cloning from GitHub..." -ForegroundColor Yellow
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to clone JUCE framework."
        exit 1
    }
    Write-Host "JUCE cloned successfully." -ForegroundColor Green
} else {
    Write-Host "JUCE already present." -ForegroundColor Green
}

# Step 6: Build
Write-Host "[6/6] Configuring and building VST3 plugin..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Write-Host "Cleaning previous build..."
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

Write-Host ""
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
Write-Host "Generator: $CMakeGenerator"
Write-Host "Architecture: $CMakeArch"
Write-Host ""

cmake .. -G "$CMakeGenerator" -A $CMakeArch -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Set-Location ..
    Write-Error "CMake configuration failed."
    exit 1
}

Write-Host ""
Write-Host "Building VST3 plugin... This may take several minutes." -ForegroundColor Yellow
Write-Host ""

$Jobs = $env:NUMBER_OF_PROCESSORS
cmake --build . --config Release --target THDAnalyzerPlugin_VST3 -j $Jobs
if ($LASTEXITCODE -ne 0) {
    Set-Location ..
    Write-Error "Build failed."
    exit 1
}

Set-Location ..

# Summary
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

$PluginPath = "$Vst3Dir\THDAnalyzerPlugin.vst3"
if (Test-Path $PluginPath) {
    Write-Host "VST3 Plugin created successfully:" -ForegroundColor Green
    Write-Host "  $(Resolve-Path $PluginPath)"
    Write-Host ""
    Write-Host "The plugin bundle contains:" -ForegroundColor Yellow
    Get-ChildItem $PluginPath -Recurse | Where-Object { $_.PSIsContainer -eq $false } | ForEach-Object {
        Write-Host "  $($_.FullName.Replace((Get-Location).Path + '\', ''))"
    }
    Write-Host ""
    Write-Host "To install the plugin:" -ForegroundColor Yellow
    Write-Host "  1. Copy THDAnalyzerPlugin.vst3 to your VST3 plugins folder:"
    Write-Host "     %LOCALAPPDATA%\Programs\Common\VST3\"
    Write-Host "  2. Or copy to a custom VST3 path and add it to your DAW"
    Write-Host ""
    
    $DllFile = Get-ChildItem "$PluginPath\Contents\x86_64-win\*.dll" | Select-Object -First 1
    if ($DllFile) {
        Write-Host "Plugin details:" -ForegroundColor Yellow
        Write-Host "  File: $($DllFile.Name)"
        Write-Host "  Size: $([math]::Round($DllFile.Length / 1MB, 2)) MB"
    }
} else {
    Write-Host "WARNING: Plugin bundle not found at expected location." -ForegroundColor Red
    Write-Host "Searching for any .vst3 files in build directory..."
    Get-ChildItem -Path $BuildDir -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  Found: $($_.FullName)"
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "Build process completed." -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
