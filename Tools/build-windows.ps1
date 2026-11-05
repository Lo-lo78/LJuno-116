$ErrorActionPreference = "Stop"

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio Installer was not found. Install Visual Studio Build Tools with Desktop development with C++."
}

$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) {
    throw "The Visual Studio C++ x64 toolchain was not found. Install the Desktop development with C++ workload."
}

$developerCommand = Join-Path $visualStudio "Common7\Tools\VsDevCmd.bat"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot "build\nmake-release"

$commandLine = '"{0}" -arch=x64 && cmake -S "{1}" -B "{2}" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build "{2}"' -f $developerCommand, $projectRoot, $buildDirectory

& cmd.exe /d /s /c $commandLine
if ($LASTEXITCODE -ne 0) {
    throw "LJuno-116 build failed with exit code $LASTEXITCODE."
}

$plugin = Join-Path $buildDirectory "LJuno116_artefacts\Release\VST3\LJuno-116.vst3"
Write-Host "Build completed: $plugin"


$searchTest = Join-Path $buildDirectory "LJunoSearchTest.exe"
if (-not (Test-Path -LiteralPath $searchTest)) {
    throw "Incremental search regression test was not built."
}
& $searchTest
if ($LASTEXITCODE -ne 0) {
    throw "LJuno-116 TEST120 incremental search regression test failed (exit code $LASTEXITCODE)."
}

$freeLfoTest = Join-Path $buildDirectory "LJunoFreeLfoTransportTest.exe"
if (-not (Test-Path -LiteralPath $freeLfoTest)) {
    throw "TEST121 host Play LFO regression test was not built."
}
& $freeLfoTest
if ($LASTEXITCODE -ne 0) {
    throw "LJuno-116 TEST121 Free LFO transport test failed (exit code $LASTEXITCODE)."
}
