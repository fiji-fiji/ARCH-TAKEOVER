# Launch the map as a game (no editor UI): one window, you spawn at the front door as a Defender.
# Usage:  powershell -ExecutionPolicy Bypass -File tools\ue\play.ps1 [-Fullscreen] [-Width 1920 -Height 1080] [-AutoShot 40]
param(
    [switch]$Fullscreen,
    [int]$Width = 1920,
    [int]$Height = 1080,
    [int]$AutoShot = 0,            # dev: take a screenshot after N seconds and quit
    [string]$Engine = "D:\UE_5.8"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Root "unreal\ParisBombMap\ParisBombMap.uproject"
$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
if (-not (Test-Path $Editor)) { throw "UnrealEditor.exe not found at $Editor (pass -Engine)" }
if (-not (Test-Path (Join-Path $Root "unreal\ParisBombMap\Binaries\Win64\UnrealEditor-ParisBombMap.dll"))) {
    throw "Game module not compiled - run tools\ue\build_project.ps1 first"
}
if (-not (Test-Path (Join-Path $Root "unreal\ParisBombMap\Content\Maps\L_SaintMartin.umap"))) {
    throw "Level not built - run tools\ue\build_level.ps1 first"
}
$Args = @("`"$Project`"", "-game", "-log", "-ResX=$Width", "-ResY=$Height")
if ($Fullscreen) { $Args += "-fullscreen" } else { $Args += "-windowed" }
if ($AutoShot -gt 0) { $Args += "-pbm_autoshot=$AutoShot" }
Write-Host "== Launching game: $Editor $Args"
$p = Start-Process -FilePath $Editor -ArgumentList $Args -PassThru -Wait
Write-Host "== Game exit code: $($p.ExitCode)"
