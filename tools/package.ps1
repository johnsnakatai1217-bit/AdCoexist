# Builds the plugin and creates the release archive (dist/AdCoexist-<version>.zip).
# The archive contains red4ext/plugins/AdCoexist/ (the DLL and the license files), to be extracted into the game directory.
# Also creates dist/AdCoexist-RestoreVanilla-<version>.zip, the optional file for the restore-vanilla option.
# Nothing is written to the game directory.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

cmake --preset msvc-x64
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
cmake --build --preset release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

# Tests that run without the game. No archive is created if they fail
ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "tests failed" }

# The version comes from project(... VERSION x.y.z ...) in CMakeLists.txt
$m = Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(AdCoexist VERSION (\d+\.\d+\.\d+)'
if (-not $m) { throw "version not found in CMakeLists.txt" }
$version = $m.Matches[0].Groups[1].Value

$dll = Join-Path $root 'build\Release\AdCoexist.dll'
$stage = Join-Path $root 'build\stage'
$pluginDir = Join-Path $stage 'red4ext\plugins\AdCoexist'
$dist = Join-Path $root 'dist'
$zip = Join-Path $dist "AdCoexist-$version.zip"

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $pluginDir, $dist | Out-Null
Copy-Item $dll $pluginDir
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $pluginDir 'LICENSE.txt')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.txt') $pluginDir
if (Test-Path $zip) { Remove-Item -Force $zip }

# Compress-Archive in PowerShell 5.1 writes \ as the separator, so use the tar (bsdtar) that comes with Windows
$tar = Join-Path $env:SystemRoot 'System32\tar.exe'
& $tar -a -c -f $zip -C $stage red4ext
if ($LASTEXITCODE -ne 0) { throw "zip creation failed" }

Write-Host "Created $zip"
& $tar -t -v -f $zip

# The optional file: only restore-vanilla.txt, which turns the option on
$optionStage = Join-Path $root 'build\stage-restore-vanilla'
$optionDir = Join-Path $optionStage 'red4ext\plugins\AdCoexist'
$optionZip = Join-Path $dist "AdCoexist-RestoreVanilla-$version.zip"
if (Test-Path $optionStage) { Remove-Item -Recurse -Force $optionStage }
New-Item -ItemType Directory -Force $optionDir | Out-Null
Copy-Item (Join-Path $root 'optional\restore-vanilla.txt') $optionDir
if (Test-Path $optionZip) { Remove-Item -Force $optionZip }
& $tar -a -c -f $optionZip -C $optionStage red4ext
if ($LASTEXITCODE -ne 0) { throw "zip creation failed" }

Write-Host "Created $optionZip"
& $tar -t -v -f $optionZip
