# Rebuilds the ad catalog (plugin/src/generated/AdCatalog.inc).
# The game directory is only read, never written. The WolvenKit name lists are downloaded to build/catalog.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/catalog/build-catalog.ps1 -GameVersion 2.50 -GameDir <game dir>
#   (-WolvenKitRef sets the WolvenKit commit or branch)
param(
    [Parameter(Mandatory = $true)][string]$GameVersion,
    [Parameter(Mandatory = $true)][string]$GameDir,
    [string]$WolvenKitRef = '95074158ed971aea39fc8abc50c2574381f4aad2'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$work = Join-Path $root 'build\catalog'
New-Item -ItemType Directory -Force $work | Out-Null

# WolvenKit name lists
$base = "https://raw.githubusercontent.com/WolvenKit/WolvenKit/$WolvenKitRef/WolvenKit.Common/Resources"
foreach ($name in 'usedhashes.kark', 'tweakdbstr.kark') {
    Invoke-WebRequest -Uri "$base/$name" -OutFile (Join-Path $work $name) -UseBasicParsing
}

cmake --preset msvc-x64
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
cmake --build --preset release --target AdCoexistCatalogTool
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$out = Join-Path $root 'plugin\src\generated\AdCatalog.inc'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
& (Join-Path $root 'build\Release\AdCoexistCatalogTool.exe') `
    --game $GameDir `
    --usedhashes (Join-Path $work 'usedhashes.kark') `
    --tweakdbstr (Join-Path $work 'tweakdbstr.kark') `
    --game-version $GameVersion `
    --wolvenkit-ref $WolvenKitRef `
    --out $out `
    --report (Join-Path $work 'report.txt')
if ($LASTEXITCODE -ne 0) { throw "catalog tool failed" }
Write-Host "WolvenKit ref: $WolvenKitRef"
