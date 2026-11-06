# Compile the Unreal C++ modules (Editor target, Development) with UnrealBuildTool.
# Usage:  powershell -ExecutionPolicy Bypass -File tools\ue\build_project.ps1 [-Target ParisBombMapEditor] [-Config Development]
param(
    [string]$Target = "ParisBombMapEditor",
    [string]$Config = "Development",
    [string]$Engine = "D:\UE_5.8"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Root "unreal\ParisBombMap\ParisBombMap.uproject"
$BuildBat = Join-Path $Engine "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $BuildBat)) { throw "Unreal Engine not found at $Engine" }
Write-Host "== UBT: $Target Win64 $Config  ($Project)"
& $BuildBat $Target Win64 $Config -Project="$Project" -WaitMutex -NoHotReload
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
Write-Host "== Build OK"
