# Full pipeline: (optional) data download -> textures -> real facades -> meshes/level.json -> compile C++ -> assemble level.
# Usage:  powershell -ExecutionPolicy Bypass -File tools\ue\build_all.ps1 [-FetchData] [-SkipFacades] [-ForceReimport] [-SkipNav]
#   -FetchData     re-download OpenStreetMap (Overpass), IGN BD TOPO buildings + orthophoto and the Panoramax photo index
#   -SkipFacades   do not (re)project the street photos (keeps assets/facades/ as it is)
param(
    [switch]$FetchData,
    [switch]$FetchOsm,          # old name, same as -FetchData
    [switch]$SkipFacades,
    [switch]$ForceReimport,
    [switch]$SkipNav
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $Root
$env:PYTHONUTF8 = "1"
if ($FetchData -or $FetchOsm) {
    python -I tools\fetch_osm.py;       if ($LASTEXITCODE -ne 0) { throw "fetch_osm failed" }
    python -I tools\fetch_ign.py;       if ($LASTEXITCODE -ne 0) { throw "fetch_ign failed" }
    python -I tools\fetch_panoramax.py; if ($LASTEXITCODE -ne 0) { throw "fetch_panoramax failed" }
}
python -I tools\make_textures.py; if ($LASTEXITCODE -ne 0) { throw "make_textures failed" }
python -I tools\make_sounds.py;   if ($LASTEXITCODE -ne 0) { throw "make_sounds failed" }
if (-not (Test-Path "assets\ortho\ground.jpg") -or $FetchData -or $FetchOsm) { python -I tools\fetch_ortho.py; if ($LASTEXITCODE -ne 0) { throw "fetch_ortho failed" } }
if (-not $SkipFacades) {
    if (-not (Test-Path "data\raw\panoramax\index.json")) { python -I tools\fetch_panoramax.py; if ($LASTEXITCODE -ne 0) { throw "fetch_panoramax failed" } }
    if (-not (Test-Path "data\raw\ign_batiment.json"))    { python -I tools\fetch_ign.py;       if ($LASTEXITCODE -ne 0) { throw "fetch_ign failed" } }
    python -I tools\build_facades.py; if ($LASTEXITCODE -ne 0) { throw "build_facades failed" }
}
python -I tools\build_map.py;     if ($LASTEXITCODE -ne 0) { throw "build_map failed" }
& (Join-Path $PSScriptRoot "build_project.ps1")
$args2 = @()
if ($ForceReimport) { $args2 += "-ForceReimport" }
if ($SkipNav) { $args2 += "-SkipNav" }
& (Join-Path $PSScriptRoot "build_level.ps1") @args2
Write-Host "== ALL DONE. Double-click Play.bat, or open unreal\ParisBombMap\ParisBombMap.uproject and press Play."
