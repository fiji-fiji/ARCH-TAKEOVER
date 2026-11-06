# Run the editor-side Python script that imports meshes and assembles /Game/Maps/L_SaintMartin.
# Usage:  powershell -ExecutionPolicy Bypass -File tools\ue\build_level.ps1 [-ForceReimport] [-SkipNav]
param(
    [switch]$ForceReimport,
    [switch]$SkipNav,
    [switch]$ForceMaterials,   # recreate every material (after a material-graph change in build_level.py)
    [string]$Engine = "D:\UE_5.8"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Root "unreal\ParisBombMap\ParisBombMap.uproject"
$Script = Join-Path $Root "unreal\ParisBombMap\Scripts\build_level.py"
$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $Editor)) { throw "UnrealEditor-Cmd.exe not found at $Editor" }
$env:PBM_FORCE_REIMPORT = if ($ForceReimport) { "1" } else { "0" }
$env:PBM_SKIP_NAV = if ($SkipNav) { "1" } else { "0" }
$env:PBM_FORCE_MATERIALS = if ($ForceMaterials) { "1" } else { "0" }
$LogFile = Join-Path $Root "unreal\ParisBombMap\Saved\Logs\build_level.log"
Write-Host "== Building level with $Editor"
Write-Host "   log: $LogFile"
& $Editor "$Project" -run=pythonscript -script="$Script" -unattended -nopause -nosplash -stdout -FullStdOutLogOutput -log="build_level.log"
Write-Host "== Editor exit code: $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { throw "Level build failed (see $LogFile)" }
