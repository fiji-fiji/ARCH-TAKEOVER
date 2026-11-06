# Architecture — Paris Saint-Martin 5v5 bomb map (Unreal Engine 5.8)

Goal: a playable Counter-Strike-style 5v5 bomb-defusal FPS map of the blocks around
**347 Rue Saint-Martin, 75003 Paris**, built from real OpenStreetMap geometry and
assembled into an Unreal Engine 5.8 project without hand-editing in the editor.

Everything lives in this workspace folder. Nothing is written elsewhere except the
Unreal build intermediates (`unreal/ParisBombMap/Intermediate`, `Binaries`, `Saved`,
`DerivedDataCache`) which are regenerable and are flagged as Dropbox-ignored.

## Pipeline overview

```
OpenStreetMap (Overpass API)
      │  tools/fetch_osm.py
      ▼
data/raw/overpass.json            raw, untouched download (+ geocode.json, bbox.json)
      │  tools/osm_lib.py          parse → local metres (x east, y north, origin = home)
      │  tools/preview_osm.py      → data/preview_osm.png (design aid)
      ▼
data/map/layout.json              HAND-AUTHORED level design: playable zone, spawns,
      │                           bomb sites, street cut-offs, callouts, cover props
      │  tools/build_map.py
      ▼
data/map/level.json               every actor to place (mesh refs, transforms, gameplay markers)
data/map/meshes/*.obj + .mtl      buildings, ground, roads, sidewalks, park, props
assets/textures/*.png             procedurally generated (tools/make_textures.py)
assets/facades/<building>.jpg     REAL facades: one photo atlas per building, re-projected from
      ▲                           street-level photos (tools/build_facades.py)
      │  tools/fetch_panoramax.py  → data/raw/panoramax/index.json + img/*.jpg
      │                           (City of Paris 2024-25 survey on Panoramax, Etalab licence)
      │  tools/fetch_ign.py        → data/raw/ign_batiment.json (IGN BD TOPO: real building heights)
      │
      │  unreal/ParisBombMap/Scripts/build_level.py   (runs INSIDE UnrealEditor-Cmd, Python plugin)
      ▼
unreal/ParisBombMap/Content/      imported StaticMeshes, Materials, Textures,
                                  Maps/L_SaintMartin.umap (generated level)
unreal/ParisBombMap/Source/       C++ gameplay module (game mode, character, bomb, bots, HUD)
```

Rule of thumb: **anything binary in `Content/` is generated**; the sources of truth are
`layout.json`, the Python tools, the C++ module and the `Config/*.ini` files.

## Folder layout

| Path | Purpose |
|---|---|
| `architecture.md` | this file |
| `feature.md` | feature log — every implemented feature is recorded there with status |
| `README.md` | how to run the pipeline and open / play the map |
| `data/raw/` | downloaded OSM data (`overpass.json`), geocode result, bbox, IGN buildings (`ign_batiment.json`) |
| `data/raw/panoramax/` | street-photo inventory (`index.json`) + the full-resolution photos (`img/`, Dropbox-ignored, re-downloadable) |
| `assets/facades/` | REAL facades: one photo atlas per building (`bld_<id>.jpg`) + `index.json` (wall → atlas rectangle) |
| `assets/ortho/` | REAL ground: the IGN aerial orthophoto of the box (`ground.jpg` + `ground.json`) |
| `assets/sounds/` | synthesised WAV sound effects (`tools/make_sounds.py`) |
| `data/map/layout.json` | level design (editable) |
| `data/map/level.json` | generated actor list consumed by the Unreal build script |
| `data/map/meshes/` | generated OBJ/MTL meshes |
| `data/preview_osm.png`, `data/preview_layout.png` | top-down previews |
| `assets/textures/` | generated PNG textures (facades, roofs, asphalt, pavement, grass…) |
| `tools/` | Python pipeline: `fetch_osm.py`, `fetch_ign.py`, `fetch_panoramax.py`, `osm_lib.py`, `preview_osm.py`, `make_textures.py`, `build_facades.py` (needs Pillow + numpy), `build_map.py` |
| `tools/ue/` | PowerShell wrappers: compile (`build_project.ps1`), run the level builder (`build_level.ps1`), whole pipeline (`build_all.ps1`), launch the game standalone (`play.ps1`) |
| `Play.bat` | double-click launcher → `tools/ue/play.ps1` (runs `UnrealEditor.exe <uproject> -game`, no editor UI) |
| `unreal/ParisBombMap/` | the Unreal Engine 5.8 project |
| `unreal/ParisBombMap/Scripts/build_level.py` | editor-side Python: import meshes, make materials, assemble level |
| `unreal/ParisBombMap/Source/ParisBombMap/` | C++ gameplay module |
| `unreal/ParisBombMap/Config/` | engine / game / input settings (text, authoritative) |

## Coordinate conventions

* Map metres: origin at the front door of 347 Rue Saint-Martin, `x` east, `y` north, `z` up.
* Unreal: 1 uu = 1 cm, left-handed, `X = x·100`, `Y = −y·100`, `Z = z·100`.
  The build script applies this transform once; OBJ files are already in Unreal units
  and orientation (OBJ is exported with X east, Y south, Z up and imported with no
  axis conversion).

## Map generation (`tools/build_map.py`)

* **Buildings**: footprints (ways + stitched multipolygon relations) extruded to `height`:
  OSM `height` tag if present, else the **IGN BD TOPO measured eaves height** (`osm_lib.apply_ign_heights`:
  the IGN polygon whose centroid lies in the footprint, else the one containing most of the outline),
  else `building:levels`×3 m + 1.5, else 19 m Haussmann default. Walls get UVs
  in metres so the facade texture tiles at the right scale (one window bay = one floor = 3 m);
  the 0–4 m band uses the shopfront texture (4 m tile). Walls listed in `assets/facades/index.json`
  are emitted instead as one quad per wall with UVs into the building's **photo atlas** (material
  `M_Fac_<id>`, no shopfront band, no stone ledges: the photo shows the real ones). Roofs ≥ 12 m get a zinc mansard
  (footprint scaled 0.8 about the centroid, +2.6 m), detailed buildings ≥ 14 m get continuous
  stone balconies at the 2nd and 5th floors. Buildings within 15 m of the zone get full detail;
  those in the backdrop ring (`backdrop_margin` = 90 m) are plain extrusions; further ones are dropped.
  One OBJ per building, pivot at the footprint centroid.
* **Covered passages** (`layout.passages`): a building crossed by a passage corridor is split:
  ground floor = footprint clipped to either side of the corridor (Sutherland–Hodgman half-plane
  clips), upper floors bridge over it with a stone ceiling at the corridor height.
* **Ground**: one asphalt slab under zone + backdrop at z = 0. Roads are strips (`road.png`,
  dashed centre line, u along the road) at z = +1 cm with 12-gon joint discs at every vertex to
  fill bends; chunked into 100 m tiles. **Sidewalks** are rings around each building footprint:
  per edge, the ring width is the distance from the facade to the nearest carriageway edge
  (clamped 1.6–7 m), raised 12 cm with a kerb face; convex corners get a wedge fill. Pavement UVs
  are world-projected so overlapping coplanar rings never z-fight. Greens (park, lawns) are
  grass prisms raised 10 cm.
* **Props** (cover): parametric primitives generated in Python — cars, vans, planters,
  kiosks, Morris columns, benches, scaffolds, dumpsters, café terraces, police barriers,
  shipping containers. Placed from `layout.json`.
* **Trees**: OSM `natural=tree` nodes → two low-poly tree meshes (trunk + two canopy spheres).
* **Gameplay markers** in `level.json`: team player starts (5 per team, chosen on a 2.5 m grid
  around the spawn centre, excluding building footprints, props and points within 1.2 m of a wall),
  bomb-site boxes A/B, nav-mesh bounds, invisible blocking walls along the zone border (40 m high)
  and along each *cut-off* segment/polyline (12 m), police-barrier props along every wall where
  the ground is free, callout labels, home marker, sun angles.
* All output is in Unreal units/axes: `X = x·100`, `Y = −y·100`, yaw = −map angle. The OBJ files
  are written as (x east, y north, z up) in cm because UE's Interchange OBJ translator maps
  (x, y, z) → (x, −y, z) with no scaling and keeps the vertex order.

## Real facades (`tools/build_facades.py`)

The "Street View" layer, from open data only:

* **Source**: Panoramax (panoramax.fr, run by IGN + OpenStreetMap France). `fetch_panoramax.py` tiles
  the zone (+60 m) into 36 bbox searches (the API caps a search at 1000 items) and writes
  `data/raw/panoramax/index.json`: ~8 600 pictures, 7 600 of them from the **City of Paris 2024-25
  survey** (imajbox 3TX+ car rig: 4096×2160, 95° lens, RTK position, heading; plus imajbox 360 HD
  12288×6144 panoramas), Etalab 2.0 licence. Contributor uploads are kept as a last resort (weight 0.2)
  and contributor panoramas are dropped (bike rigs are not levelled).
* **Camera model**: position from the index (map metres), lens 2.4 m above the flat ground, heading =
  `view:azimuth`, pinhole with `F = (W/2)/tan(fov/2)` and a +3.3° pitch for the campaign frames
  (the horizon sits below the frame centre), equirectangular for the panoramas. Calibrated by eye
  with `--debug` (writes the source frames with the projected wall outline + one line per floor to
  `data/_debug/`).
* **Walls**: same cleaned rings and wall indices as `build_map.py` (`osm_lib.clean_ring`). A wall is a
  candidate when it is ≥ 2.5 m and the point 0.8 m outside it is not inside another footprint.
* **Photo choice**: cameras within 75 m on the outer side and not too grazing (cos ≥ 0.2), whose
  ground line of sight to both wall ends and the middle crosses no footprint edge. On a 7×7 grid over
  the wall the photos are added greedily by the pixel density they add (near frames cover the
  shopfront, frames 20–40 m back and the panoramas cover the upper floors), up to 10 per wall.
  Per texel (40 px/m) the best-scoring photo wins (density × incidence × frame-centre weight, with a
  ×1.4 hysteresis so a new photo must be clearly better: fewer seams); holes are filled with the column
  average; walls under 45 % coverage keep the procedural facade.
* **Atlas**: the walls of one building are shelf-packed into a 4096-wide JPEG (height = next power of
  two; the px/m is reduced until it fits). `index.json` records the pixel rectangle per wall index;
  `build_map.py` maps `z ∈ [0, h]` onto it (OBJ v = 0 at the bottom; Unreal flips v on import).
* **Limits**: photos bake in parked cars, people, trees and the lighting of the survey day; positions
  are accurate to ~2 m so adjacent frames can disagree by a window width at the seams; courtyard and
  party walls stay procedural.

## Real ground (`tools/fetch_ortho.py`)

The IGN high-resolution aerial orthophoto (`HR.ORTHOIMAGERY.ORTHOPHOTOS`, open licence) is fetched as 2048 px
WMS tiles for the backdrop box at 8 px/m and stitched into `assets/ortho/ground.jpg` (+ `ground.json` with the
map-metre bbox). `build_map.py` gives every horizontal ground polygon (the asphalt slab, road strips and joint
discs, sidewalk tops and corner wedges) world-projected UVs into it (`MeshWriter.ortho_cap`, material
`M_Ground`); greens keep the grass (their canopy would otherwise be baked into the lawn). `build_level.py` builds
`M_Ground` as ortho × (asphalt detail × 2) with the detail tiled every 4 m (`detail_tiling`), so the aerial photo
gives the colour and markings and the tiling texture the grain at eye level. Parked cars and tree canopies are
baked in from above (limitation).

## Generated models and sounds

* `build_map.py` also emits two unplaced meshes: `soldier.obj` (1.8 m, boxes + spheres: legs, boots, torso in
  `M_Uniform` = slot 0 tinted per team, plate carrier, head, helmet, forward arms, and the rifle in the hands)
  and `rifle.obj` (receiver, handguard, barrel, magazine, grip, stock, sights, rail; +x forward, origin at the
  grip). `AFPSCharacter` finds `/Game/Meshes/soldier` and `/Game/Meshes/rifle` in its constructor; when they
  exist the primitive head / 3P gun components are hidden, the body mesh sits at −capsule half-height (and is
  Z-squashed when crouched), the 1P rifle hangs under the camera at (22, 13, −19) cm, scale 0.85.
* `tools/make_sounds.py` synthesises 18 WAVs with numpy (noise bursts, sweeps, one-pole filters, ADSR
  envelopes, tanh saturation) into `assets/sounds/`; `build_level.py` imports them to `/Game/Sounds`.
  `GameSounds` (`GameSounds.h/.cpp`) loads them by name on first use (missing = silent + one warning) and
  wraps `PlaySoundAtLocation` / `PlaySound2D`. Cues: rifle shot (3-D, random pitch), dry fire, reload,
  footsteps every 1.7 m (volume by stance), hit / headshot (2-D, shooter only), death, interaction ticks,
  bomb planted / defused, bomb beep on each blink (pitch rises as the timer runs out), explosion, round start /
  win / lose, match end.

## Unreal project

* Engine: 5.8 at `D:\UE_5.8`. Plugins: PythonScriptPlugin, EditorScriptingUtilities,
  Interchange (OBJ import). Modules: `ParisBombMap` (Runtime: Core, Engine, AIModule,
  NavigationSystem) and `ParisBombMapEditor` (Editor: UnrealEd) — the editor module exposes
  `UMapBuilderLibrary` to Python (spawn volumes with a real cube brush via the actor-factory brush
  helper, build navigation, set complex-as-simple collision).
* Input: legacy `DefaultInput.ini` axis/action mappings (text-only, no Enhanced Input assets);
  `DefaultPlayerInputClass`/`DefaultInputComponentClass` pinned to the legacy classes.
* Rendering: tuned for the machine's GeForce GTX 1660 (no RT cores): Lumen GI / reflections OFF,
  `r.GenerateMeshDistanceFields=False` (with Lumen every re-imported mesh costs a ≈ 13 s CPU distance-field
  build — 25 min after a full reimport), classic shadow maps, screen-space reflections, TAA; the sky light
  (real-time capture) provides the ambient term. Four values in `DefaultEngine.ini` switch Lumen + VSM + TSR
  back on for a stronger card. Nanite enabled but meshes imported as regular static meshes. All lights are
  movable; no lightmass build is ever needed. Photo facades are plain base-colour textures (one JPEG atlas
  per building → `T_bld_<id>` / `M_Fac_<id>`), roughness 0.85.
* Nav mesh: Recast with `RuntimeGeneration=Dynamic` (bots work even if the editor-time build is
  skipped), tile pool enlarged for the 345 × 415 m zone.
* Visual stand-ins: characters are engine basic shapes (cylinder body + sphere head + cube rifle)
  tinted by team material; the C4 is a cube with a blinking red point light. Bullet tracers /
  impacts use debug-draw lines (visible in editor and Development builds).
* Everything runs on one machine (one human + bots); nothing is replicated.
* Gameplay C++ (module `ParisBombMap`):
  * `ABombGameMode` — round loop (freeze → live → post), team assignment, win conditions,
    bot spawning to fill 5v5, score, round timer, bomb timer.
  * `ABombGameState` — replicated round state, scores, bomb state, for HUD.
  * `ABombPlayerState` — team id.
  * `ATeamPlayerStart` — PlayerStart with team tag.
  * `AFPSCharacter` — first-person pawn, health, hitscan rifle, plant/defuse interaction.
  * `AFPSPlayerController` — input bindings.
  * `ABombSite` — box trigger, site id A/B.
  * `ABombActor` — planted bomb with timer / defuse progress.
  * `ABotController` — AIController: roams between waypoints (spawn → sites), engages
    visible enemies, attackers try to plant, defenders try to defuse.
  * `ABombHUD` — canvas HUD: crosshair (bloom-aware, hit marker), health bar, ammo, round / bomb timer with scores and alive counts, phase banners, kill feed, interaction progress bar and hints, nearest callout, death/spectate notice, Tab scoreboard.
  * `ACalloutMarker` — named world location with a text render.
* Level: `/Game/Maps/L_SaintMartin` generated by `Scripts/build_level.py`:
  1. import PNGs -> `/Game/Textures/T_*` (AssetImportTask)
  2. create `/Game/Materials/M_*` with `MaterialEditingLibrary` (texture or constant colour -> base colour, constants -> roughness / metallic / emissive; two-sided), plus team + bomb materials
  3. import every OBJ through Interchange with a duplicated `DefaultAssetsPipeline` (`/Game/Pipelines/ObjPipeline`: no materials/textures, combine all, no collision, use source name), assign materials by `usemtl` slot name, set complex-as-simple collision, save
  4. new level -> spawn StaticMeshActors (folders: building / road / sidewalk / green / prop / tree), hidden cube blockers, `ATeamPlayerStart`s, `ABombSite`s, `ACalloutMarker`s, NavMeshBoundsVolume, sun / sky atmosphere / sky light / height fog / clouds / post-process, a warm light at the front door
  5. save, build navigation (the commandlet world needs the nav-build locks removed and a few
     nav-system ticks before/after `Build()`; generation is synchronous and takes ~14 min because
     the collision is the full render geometry), save again.

## Build / run

See `README.md`. In short: `tools/ue/build_all.ps1` runs the Python pipeline, compiles the
C++ module with UnrealBuildTool, then runs `UnrealEditor-Cmd.exe … -run=pythonscript` to
assemble the level. Open `unreal/ParisBombMap/ParisBombMap.uproject` and press Play.
