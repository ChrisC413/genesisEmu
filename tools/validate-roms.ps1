# Runs the [rom] tests (label "rom") against the user's own ROM dumps.
#
#   $env:GENESIS_ROM_DIR = 'D:\roms'; tools\validate-roms.ps1 [-Preset win-release]
#
# Exits non-zero if the build or any ROM test fails, or if GENESIS_ROM_DIR is not set.

param([string]$Preset = 'win-release')

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

if (-not $env:GENESIS_ROM_DIR -or -not (Test-Path -PathType Container $env:GENESIS_ROM_DIR)) {
    Write-Error 'Set GENESIS_ROM_DIR to the directory holding your ROM dumps.'
    exit 2
}

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --preset $Preset -L rom -V
exit $LASTEXITCODE
