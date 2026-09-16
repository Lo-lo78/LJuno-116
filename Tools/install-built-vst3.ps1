$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$builtContents = Join-Path $projectRoot "build\nmake-release\LJuno116_artefacts\Release\VST3\LJuno-116.vst3\Contents"
$installedContents = "C:\Program Files\Common Files\VST3\LJuno-116\Contents"

Copy-Item -LiteralPath (Join-Path $builtContents "x86_64-win\LJuno-116.vst3") `
          -Destination (Join-Path $installedContents "x86_64-win\LJuno-116.vst3") -Force
Copy-Item -LiteralPath (Join-Path $builtContents "Resources\moduleinfo.json") `
          -Destination (Join-Path $installedContents "Resources\moduleinfo.json") -Force
