# PARIS MAP — Feature Log

> One entry per implemented feature: what it does, where it lives, how to test it.
> System design lives in [architecture.md](architecture.md). Nothing is implemented without a line here.

## Status — Saint-Martin 5v5 bomb map (Unreal Engine 5.8)

Legend: ✅ done (verified in a run / on a screenshot) · 🟡 built, not yet verified or partial · ❌ not done / out of scope.

| #  | Feature                                                              | Status |
|----|----------------------------------------------------------------------|--------|
| D1 | Geocode the home address (Nominatim)                                 | ✅ done (48.86828 N, 2.35505 E) |
| D2 | OpenStreetMap download, ~670 m × 670 m around home (Overpass)        | ✅ done (938 buildings, 1 083 road ways, 439 trees, 1 183 POIs) |
| D3 | OSM parser → local metres (heights, widths, parks, trees, barriers, multipolygons) | ✅ done |
| D4 | Pure-Python PNG canvas (no Pillow on this machine)                   | ✅ done |
| D5 | Top-down design preview with street names + 50 m grid                | ✅ done (`data/preview_osm.png`) |
| D6 | Level design file (zone, spawns, sites, cut-offs, passages, callouts, props) | ✅ done (`data/map/layout.json`) |
| D7 | Map builder: OBJ meshes + `level.json` actor list                    | ✅ done (669 meshes / 147 k tris / 1 191 actors) |
| D8 | Procedural textures (facade, shopfront, zinc, asphalt, road, pavement, grass, tarp) | ✅ done (8 PNGs) |
| D9 | Real building heights from IGN BD TOPO (French open data)            | ✅ done (611 of 938 buildings matched) |
| D10| Real facades: street-level photos (Panoramax, City of Paris campaign) projected onto every street-facing wall | ✅ verified in game (232 buildings / 726 walls, `data/screenshot_facades.png`) |
| D11| Real ground: IGN aerial orthophoto on roads and pavements, blended with asphalt detail | ✅ verified in game (`data/screenshot_ground.png`: real markings, zebra crossings) |
| L1 | Playable zone 345 m × 415 m — classic 5v5 proportions                | ✅ done (enlarged at user request) |
| L2 | Spawns: attackers at Rue Réaumur / Arts-et-Métiers, defenders at the front door | ✅ done |
| L3 | Bomb sites: A = Nazareth / Passage du Vertbois, B = Square Émile-Chautemps | ✅ done |
| L4 | Three lanes (Sébastopol / Rue Saint-Martin / Vaucanson–Vertbois–Passage) + connectors | ✅ done |
| L5 | Zone walls + cut-off walls, police barriers auto-placed along them   | ✅ done (16 walls, 178 barriers) |
| L6 | 25 callouts (world text, HUD, bot waypoints)                         | ✅ done |
| L7 | Cover props on every lane (cars, buses, kiosks, scaffolding, containers…) | ✅ done (54 placed) |
| L8 | Covered passages carved through buildings                            | ✅ done (Passage du Vertbois) |
| U1 | UE 5.8 C++ project scaffold (game + editor modules, ini config)      | ✅ done (compiles clean) |
| U2 | First-person character: movement, crouch, sprint, hitscan rifle, damage, spectate | ✅ verified (standalone run + screenshot) — human feel to playtest |
| U3 | Bomb-defusal game mode: freeze / live / plant / defuse / explode, scores, side swap, first to 13 | ✅ verified (round loop reaches Live) — plant / defuse not yet seen in a run |
| U4 | Bots on the nav mesh: attack sites, hold, fight, plant, defuse, pick up the bomb | ✅ verified (bots reach the sites and advance up the lanes) |
| U5 | Canvas HUD: crosshair, HP, ammo, timer, scores, banners, kill feed, callout, scoreboard | ✅ verified (screenshot) |
| U6 | Editor build script: textures → materials → meshes → level → nav mesh | ✅ verified (level 4 s, nav 4 096 tiles) |
| U7 | Build wrappers (`build_project.ps1`, `build_level.ps1`, `build_all.ps1`) | ✅ done |
| U8 | One-click game launcher (`Play.bat` → `tools/ue/play.ps1`, no editor UI) | ✅ verified (standalone run + screenshot) |
| U9 | Renderer tuned for the GeForce GTX 1660 (no Lumen → no CPU distance-field builds, shadow maps, SSR, TAA) | ✅ verified (launch 25 min → 90 s after a reimport) |
| U10| Soldier + rifle models (generated low-poly meshes replacing the cylinder / sphere / cube stand-ins; 1P rifle with sights) | ✅ verified in game (`data/screenshot_soldier.png`) |
| U11| Sound: synthesised rifle, reload, footsteps, hit, plant / defuse beeps, bomb timer, explosion, round jingles | ✅ built — 18 WAVs imported, no missing-sound warnings in a run; to hear in a playtest |
| U12| No depth of field (FPS readability), dev switch `-pbm_lookmate` aims the auto-screenshot at a teammate | ✅ done |
| U13| HUD readability: every text sized in pixels of a 1080p screen (22–64 px), callout labels face the player | ✅ verified (`data/screenshot_hud.png`) |
| G1 | Weapon roster ported from FPS Reborn (rules only): 5 sidearms, 4 main guns, 2 snipers, knife — fire rate, magazine, reload, head / body / legs damage, fall-off, spread model, recoil pattern, silencer, scope | ✅ built — all 11 guns + knife in game, bots buy and hold them (run log) |
| G2 | Buy menu (B in the buy phase): cards per slot, mouse buy, right-click sell back, money / next-round minimum / countdown | ✅ verified (`data/screenshot_shop.png`) |
| G3 | Economy: start 800, round win 3 000, loss 1 900 / 2 400 / 2 900 by streak, kill 200, plant 300, cap 9 000; survivors keep their guns, halftime reset | ✅ verified (run log: $800 → $2 700 after a lost round, winners $3 800 → rifles + kevlar) |
| G4 | Round flow as in FPS Reborn: 20 s buy phase (5 s in round 1) walking inside the spawn fence, 100 s round, 5 s result, first to 13 win-by-two, halftime after 12, overtime swaps every round, a plant in progress may finish at 0:00 | 🟡 built — buy phase, spawn fence, 100 s rounds seen; win-by-two / overtime not yet reached in a run |
| G5 | Kevlar: light +25 / heavy +50 armour soaks damage first | ✅ built (bots show kevlar 25 / 50 in the run log) |
| G6 | Grenades: smoke (15 s cloud), flash (blinds enemies looking at it), frag (6 m blast); 2 / 2 / 1 per round; Q / E / C quick-throw, 5 cycles | 🟡 built — bots carry smokes / flashes / frags; detonations not yet observed in a run |
| G7 | Dropped guns: the main gun falls when you die, F picks it up or swaps, bots grab drops when unarmed; cleared each round | 🟡 built — not yet observed in a run |
| G8 | Bomb carry: 4 selects the bomb; as an attacker it waits on the ground next to you at round start, bot teammates only take it after 10 s / when you leave spawn / when you die; hold 4 to plant / defuse | 🟡 built — bot carrier verified; the human hand-off needs a playtest as attacker (M) |
| G9 | Bots buy with the FPS Reborn logic (save when poor, rifles + kevlar when rich, pistol-round sidearms), use smokes / flashes on site hits and frags on seen enemies | ✅ verified (run log: pistol-round sidearms, kevlar, rifles in round 2, grenades) |
| G10| HUD extensions: money + gain popup, weapon name and slot, armour, grenade pips, scoreboard money, kill feed with weapon | ✅ verified (`data/screenshot_live.png`, `data/screenshot_round2.png`) |
| G11| Generated gun models for the roster (realistic silhouettes, no flowers): pistols, revolver, machine pistol, sawn-off, SMG, pump shotgun, rifles, scout, long rifle, knife, three grenades | ✅ built — 16 meshes imported; the P9 is visible in first person |
| A1 | Parisian building Blender asset (user-supplied 3-piece Haussmann block) split by Blender into 8 textured pieces (3 intact blocks, 3 shelled ruins, 2 facade slabs) and imported as FBX | ✅ built — pieces previewed in Blender, imported with textures (`/Game/Models/parisian_block`) |
| A2 | War-zone dressing: 7 A1 pieces placed (B site ruin, Sébastopol chokepoint, facade slabs), 53 props (rubble, sandbags, hedgehogs, jersey barriers, burnt cars, craters, smoke columns), overcast dusty mood — photo facades untouched | ✅ built — see `data/screenshot_warzone_*.png` |
| —  | Online play (replication)                                            | ❌ out of scope for now |

---

## D1. Geocode the home address — ✅
**What:** turns "347 Rue Saint-Martin, 75003 Paris" into the map origin. Everything else is measured in metres from
this point (x east, y north, origin = the front door).
**How:** `tools/fetch_osm.py` queries Nominatim once and caches the answer in `data/raw/geocode.json`.
**Test:** `data/raw/geocode.json` holds lat 48.8682801 / lon 2.3550525.

## D2. OpenStreetMap download — ✅
**What:** every building, road, footway, park, tree, barrier and POI in a ~670 m × 670 m box around home.
**How:** `tools/fetch_osm.py` builds the bounding box (`data/raw/bbox.json`) and runs one Overpass query →
`data/raw/overpass.json`. Re-run with `build_all.ps1 -FetchOsm`.
**Test:** the script prints the element counts (938 buildings, 1 083 road ways, 439 trees, 1 183 POIs).

## D3. OSM parser to local metres — ✅
**What:** the raw JSON becomes clean polygons / polylines in metres: buildings with heights (from `height` /
`building:levels`, Haussmann defaults otherwise), roads with widths per highway class, parks, trees, barriers, POIs;
multipolygon relations are stitched into rings.
**How:** `tools/osm_lib.py` (`project`, `load`, `stitch`, `poly_area`, `point_in_poly`, `dist_point_seg`,
`ROAD_WIDTH`, `parse_height`).
**Test:** `python -I tools\preview_osm.py` renders without errors; buildings close, streets have the right width.

## D4. Pure-Python PNG canvas — ✅
**What:** a tiny raster library so previews and textures can be written without Pillow (not installed here).
**How:** `tools/pnglib.py` — `Canvas` with `rect`, `polygon` (scanline fill), `line`, `polyline`, `circle`,
5×7 bitmap `text`, zlib PNG encoder.
**Test:** `data/preview_*.png` and `assets/textures/*.png` open in any viewer.

## D5. Top-down design preview — ✅
**What:** a labelled top-down plan of the neighbourhood (street names, 50 m grid) with an optional overlay of the
level design (zone, spawns, sites, props) so the layout can be judged before building anything.
**How:** `tools/preview_osm.py` → `data/preview_osm.png`, `data/preview_layout.png`.
**Test:** run it; spawns and sites sit on streets, not inside buildings.

## D6. Level design file — ✅
**What:** the single source of truth for the map design: `zone` polygon, `backdrop_margin`, `spawnT` / `spawnCT`
(centre, radius, facing), `siteA` / `siteB` (centre, half-extents), `home`, `cutoffs`, `passages`, 25 `callouts`,
54 `props` (type, position, rotation).
**How:** `data/map/layout.json`, read by `tools/build_map.py`.
**Test:** edit a value, re-run `build_map.py`, see it in `data/preview_level.png`.

## D7. Map builder — ✅
**What:** generates the whole level geometry and the actor list Unreal assembles:
- buildings as extruded footprints with a 4 m shopfront band, Haussmann facade tiling (3 m bays), balcony ledges and
  zinc mansard roofs; 267 detailed in the zone + 274 flat backdrop buildings in the 90 m ring outside it;
- roads as strips from the OSM centrelines with joint discs at junctions; sidewalk rings sized from the nearest
  carriageway; world-projected UVs so nothing z-fights;
- Square Émile-Chautemps as a grass polygon, trees as trunk + crown;
- 17 parametric prop meshes (car, van, RATP bus, planter, kiosk, Morris column, bench, scaffold + tarp, dumpster,
  café terrace, police barrier, shipping container…);
- covered passages carved through the ground floor of the building above (Sutherland–Hodgman clipping, see L8);
- invisible blocker walls, 5 + 5 spawn points, 2 bomb-site boxes, callout markers, nav-mesh bounds, sun direction.
**How:** `tools/build_map.py` (`MeshWriter`, ear-clipping `triangulate`, `clean_ring`, `barriers_along`,
`free_spots`, `wall_blocker`, `ue_loc` / `ue_yaw`) → `data/map/meshes/*.obj` + `.mtl` (centimetres, Unreal axes)
and `data/map/level.json`.
**Test:** `python -I tools\build_map.py` prints 669 meshes / 147 k triangles / 1 191 actors and writes
`data/preview_level.png`.

## D8. Procedural textures — ✅
**What:** 8 tiling textures sized in real metres so they map 1:1 on the meshes: `facade_haussmann` (3 m bay with
window, shutters, railing), `shopfront` (4 m), `roof_zinc`, `asphalt`, `road` (dashed centre line), `pavement`,
`grass`, `tarp`.
**How:** `tools/make_textures.py` → `assets/textures/*.png` (drawn with `pnglib`).
**Test:** run it; in Unreal the facade windows line up floor to floor.

## D9. Real building heights (IGN BD TOPO) — ✅
**What:** the French national mapping agency publishes every building with its measured height (`hauteur`, eaves
height from the ground), storey count and ground altitude under an open licence. Those replace the estimated
heights (`building:levels` × 3 m or the 19 m Haussmann default) for every building OpenStreetMap does not tag
with an explicit `height`. Your building went from 22.5 m (7 levels × 3 + 1.5) to the measured 18.1 m.
**How:** `tools/fetch_ign.py` → WFS `BDTOPO_V3:batiment` for the zone → `data/raw/ign_batiment.json`
(1 074 buildings, 1 015 with a height); `osm_lib.apply_ign_heights` matches each OSM footprint to the IGN
polygon whose centroid falls inside it (largest wins), else to the IGN polygon containing most of its outline.
**Test:** `python -I tools\fetch_ign.py` then `build_map.py`; 611 of the 938 OSM buildings get an IGN height
(the rest are courtyards / tiny annexes with no IGN counterpart).

## D10. Real facades from street-level photos (Panoramax) — ✅
**What:** the "Street View" step, with open data: Panoramax (the French open street-imagery platform run by IGN and
OpenStreetMap France) holds the City of Paris 2024–25 survey of the neighbourhood — 4096 × 2160 frames every few
metres with RTK position, heading and a 95° lens, plus 360° panoramas — under the Etalab open licence. Every
street-facing wall of every detailed building is textured by re-projecting those photos onto the real footprint:
for each wall texel the photo that sees it best (in frame, not hidden behind another building, highest pixel
density, least grazing angle) is sampled, so shopfronts come from the near frames and upper floors from frames
further back or from the panoramas. Walls no photo sees keep the procedural facade.
**How:** `tools/fetch_panoramax.py` (inventory → `data/raw/panoramax/index.json`: 8 647 pictures, 7 599 from the
City survey; photos are downloaded on demand into `img/`, 2.6 GB, Dropbox-ignored), `tools/build_facades.py`
(projection → one JPEG atlas per building in `assets/facades/`, 46 MB in all; resumable per building, run as
`--shard k/N` in parallel, `--repack` tightens the atlases, `--debug` draws the projected wall on the source
frames for calibration), `build_map.py` emits photo-textured walls as plain quads with atlas UVs (no procedural
shopfront band / ledges on those walls), `build_level.py` imports the atlases (`texture_path` materials) and
re-imports any mesh or texture whose source file is newer than its asset.
**Result:** 232 of the 267 detailed buildings textured, 726 walls, 3 084 photo uses, mean wall coverage 90 %;
3 × 89 buildings in ≈ 15 min on 3 processes (photo download included). The 35 untextured buildings are inner
courtyards / backs with no street camera.
**Test:** `data/preview_facades.jpg` contact sheet; `python -I tools\build_facades.py --only w56319987 --debug`
shows the projection on the frames (`data/_debug/`); in game, the boulangerie and your door are recognisable.
**Calibration notes:** campaign pitch +3.3° (the horizon sits below the frame centre), lens height 2.4 m;
contributors' panoramas are excluded (not levelled: they tilted the upper floors in the first test).

## D11. Real ground from the IGN orthophoto — ✅
**What:** the ground (asphalt slab, road strips, sidewalk rings, the square) is textured with the IGN
high-resolution aerial orthophoto (open licence, ~20 cm/px): real road markings, zebra crossings, bus lanes,
the square's paths. Because 20 cm/px is blurry at eye level, the material multiplies it with the tiling
asphalt detail texture. Trees' canopies and parked cars are baked in from above (known limitation).
**How:** `tools/fetch_ortho.py` → WMS `HR.ORTHOIMAGERY.ORTHOPHOTOS` (data.geopf.fr) for the backdrop box →
`assets/ortho/ground.jpg` (+ `ground.json` with the bbox); `build_map.py` gives the ground, road and
sidewalk polygons world-projected UVs into it (material `M_Ground`); `build_level.py` builds `M_Ground`
as ortho × detail.
**Test:** `data/preview_ortho.jpg`; in game the zebra crossing at Réaumur and the bus lane markings are real.
**Notes:** the stitched sheet (525 × 595 m at 8 px/m) is resampled to 4096 × 4096 so Unreal builds mipmaps for
it (a non-power-of-two texture had no mips and shimmered at grazing angles); the blend is ortho × (0.7 + 0.6 ×
detail), a straight ×2 multiply looked blotchy. Verified on `data/screenshot_ground.png`.

## L1. Playable zone — ✅
**What:** 345 m × 415 m (x ∈ [−195, 150], y ∈ [−320, 95] around home), the proportions of a classic 5v5 bomb map,
with a 90 m ring of backdrop buildings outside the walls so the skyline continues.
**How:** `zone` + `backdrop_margin` in `layout.json`. The first draft (285 m × 275 m) felt small; enlarged on
2026-11-06 at the user's request.
**Test:** `data/preview_level.png`; walking spawn to spawn takes ≈ 60 s.

## L2. Spawns — ✅
**What:** attackers start on Rue Réaumur at Rue Saint-Martin / métro Arts-et-Métiers (−82, −262), facing north.
Defenders start in front of 347 Rue Saint-Martin (6, −12) — you defend your own front door.
**How:** `spawnT` / `spawnCT` in `layout.json`; `build_map.py` picks 5 free spots per team inside the radius and
validates them against building footprints; `ATeamPlayerStart` actors carry `TeamId` + `SpawnLabel`.
**Test:** press Play — you stand at the door; Tab shows 5 per side.

## L3. Bomb sites — ✅
**What:** A = Rue Notre-Dame-de-Nazareth at the mouth of the Passage du Vertbois (82, −58), B = Square
Émile-Chautemps (−88, −124). Distances: CT→A ≈ 95 m, CT→B ≈ 160 m, T→B ≈ 160 m, T→A ≈ 300 m by mid / 365 m by the
east lane.
**How:** `siteA` / `siteB` in `layout.json` → `ABombSite` box actors (`IsInside`, `RandomPointInside`).
**Test:** carry the bomb into a site and hold E — the "PLANTING" progress bar appears.

## L4. Lanes — ✅
**What:** West = Boulevard de Sébastopol → B / Nazareth-West; Mid = Rue Saint-Martin → Mid Plaza → Home;
East = Rue Vaucanson → Rue du Vertbois → Passage du Vertbois → A. Connectors: Rue Papin, Rue Salomon-de-Caus,
Rue Meslay, Rue N-D-de-Nazareth, Rue Blondel, Rue Sainte-Apolline.
**How:** the real streets, kept open by the zone and cut-off choices in `layout.json`.
**Test:** bots take all three lanes in a run (positions are logged with `-pbm_autoshot`).

## L5. Walls and barriers — ✅
**What:** invisible 40 m walls on the zone perimeter plus cut-off walls along the west building line of Sébastopol
and across Rue Saint-Martin south of Réaumur; police-barrier props are placed automatically along every wall where
the ground is free so the limits read in-game.
**How:** `cutoffs` in `layout.json`; `wall_blocker` + `barriers_along` in `build_map.py`; blockers are hidden
cubes with `BlockAll` in the level.
**Test:** 16 blocker walls, 178 barriers in `level.json`; you cannot leave the zone.

## L6. Callouts — ✅
**What:** 25 named spots (Métro, Mid Plaza, Home, Passage, Square, Nazareth-West…) shown as floating world text;
the HUD prints the nearest one and bots use them as random detour waypoints.
**How:** `callouts` in `layout.json` → `ACalloutMarker` (`UTextRenderComponent`, world size 55);
`ABombGameMode::NearestCallout`.
**Test:** walk around, the HUD callout changes. Known: the text is mirrored from behind.

## L7. Cover props — ✅
**What:** 54 hand-placed props on every lane: parked cars / vans / RATP buses, kiosks, Morris columns, planters,
benches, scaffolding with tarp, dumpsters, café terraces, shipping containers, police barriers.
**How:** `props` in `layout.json` (type, `at`, `rot`), positions validated against footprints by `build_map.py`.
**Test:** `data/preview_level.png` shows them; none clips into a building.

## L8. Covered passages — ✅
**What:** the Passage du Vertbois is a real covered passage (OSM building r550484, layer 1 above it). A 3.6 m × 4 m
corridor is carved through the ground floor so the east lane runs through it into A.
**How:** `passages` in `layout.json`; `in_passage` / `passage_hits` / `clip_halfplane` in `build_map.py`;
`inside_any_building` is passage-aware so props and spawns may sit inside.
**Test:** walk the east lane from Rue du Vertbois to A without touching a wall.

## U1. Unreal project scaffold — ✅
**What:** `unreal/ParisBombMap/ParisBombMap.uproject` (UE 5.8), runtime module `ParisBombMap`, editor module
`ParisBombMapEditor` (`UMapBuilderLibrary`: `SpawnBoxVolume`, `BuildNavigation`, `SetComplexCollision`,
`GetTriangleCount`), `DefaultEngine.ini` (default map `L_SaintMartin`, `BombGameMode`, dynamic Recast nav mesh,
2 000 UU tiles, 4 096-tile pool, BombSite collision profile), `DefaultInput.ini` (legacy axes / actions),
`DefaultGame.ini`.
**How:** `tools/ue/build_project.ps1` runs UnrealBuildTool (Development Editor).
**Test:** build succeeds with no warnings (≈ 211 s clean, far less incremental).

## U2. First-person character — ✅
**What:** `AFPSCharacter` shared by the human and the bots: WASD + mouse (FOV 95), sprint (Shift), jump, crouch
(Ctrl / C), hitscan rifle (30 / 90 rounds, 0.095 s fire interval, 30 body / 100 head damage, bloom that grows with
movement, recoil, muzzle-flash light, debug-line tracers), 100 HP, no friendly fire, death → hidden + spectate a
teammate, reset per round, frozen during freeze time. Body = team-coloured engine cylinder + sphere + cube; a slim
cube rifle in first person.
**How:** `Source/ParisBombMap/FPSCharacter.*`, `FPSPlayerController.*`.
**Test:** verified in a standalone `-game` run (auto-screenshot at 40 s): HUD, movement, rifle. Human feel still to
playtest.

## U3. Bomb-defusal game mode — ✅ (plant / defuse not yet seen in a run)
**What:** `ABombGameMode` + `ABombGameState`: 5 v 5, 4 s warm-up → 8 s freeze → 2:30 live; a random attacker
carries the bomb (`ABombActor`: blinking light, dropped on death, picked up by walking over it); plant = hold E 4 s
inside A / B, defuse = hold E 7 s next to it, 45 s fuse with a blast radius; round won by elimination / explosion /
defuse / time-out; scores, half-time side swap after round 12, first to 13, match restart; M switches your team next
round; `-pbm_autoshot=<s>` takes a screenshot, logs every character's position and quits.
**How:** `BombGameMode.*`, `BombGameState.*`, `BombActor.*`, `BombPlayerState.h`, `BombTypes.h`.
**Test:** verified: 10 characters, round loop reaches Live, timers tick, feed shows "GO GO GO". Plant / defuse /
explosion not yet observed — be the carrier (M to attack) and hold E in a site.

## U4. Bots — ✅
**What:** `ABotController` fills every empty slot: nav-mesh travel to a random attack site with random callout
detours, 3 / 2 defender split across A and B, hold + look around, line-of-sight detection in an 80° cone, reaction
time and aim error from a per-bot skill value, burst fire, strafing, plant / defuse, bomb-carrier pickup, stuck
detection, sprint on long trips. Think tick every 0.15 s.
**How:** `BotController.*`; nav mesh baked by the level build (U6) and regenerated at runtime (Dynamic) with a
`Nav->Build()` safety net in `BeginPlay`.
**Test:** verified at 40 s: three defenders hold A, one covers B, attackers ≈ 100 m up Rue Saint-Martin / the east
lane with the carrier.

## U5. HUD — ✅
**What:** `ABombHUD` on a Canvas: bloom-aware crosshair (rect with outline) + hit marker, HP bar, ammo, round / bomb
timer with team scores and alive counts, phase banners with the objective, kill feed, interaction progress bar +
hints, nearest callout, damage flash, death / spectate notice, Tab scoreboard, controls reminder.
**How:** `BombHUD.*`.
**Test:** verified on `data/screenshot_ingame.png`.

## U6. Editor build script — ✅
**What:** `Scripts/build_level.py`, run inside `UnrealEditor-Cmd` by `tools/ue/build_level.ps1`: imports the
8 textures (`/Game/Textures/T_*`), creates 32 materials (`/Game/Materials/M_*`, incl. team colours and the emissive
bomb), imports the 669 OBJ meshes through Interchange with a duplicated pipeline (`/Game/Pipelines/ObjPipeline`,
combine all, complex-as-simple collision), assembles `L_SaintMartin` from a blank map (1 191 mesh actors in folders,
16 blockers, 10 team starts, 2 sites, 25 callouts, nav bounds, sun / sky atmosphere / sky light / fog / clouds /
post-process, door light), bakes the nav mesh via `UMapBuilderLibrary::BuildNavigation`, saves everything.
**How:** env `PBM_FORCE_REIMPORT` (`-ForceReimport`) / `PBM_SKIP_NAV` (`-SkipNav`).
**Test:** first run ≈ 16 min import + 14 min nav; cached rebuild 4 s; log ends with "nav tiles: 4096";
`L_SaintMartin.umap` ≈ 4.2 MB.

## U7. Build wrappers — ✅
**What:** `tools/ue/build_project.ps1` (UBT), `tools/ue/build_level.ps1` (editor commandlet),
`tools/ue/build_all.ps1` (textures → map → compile → level, `-FetchOsm`, `-SkipNav`, `-ForceReimport`).
Unreal intermediates (`Intermediate`, `Binaries`, `Saved`, `DerivedDataCache`) are flagged Dropbox-ignored.
**Test:** `powershell -ExecutionPolicy Bypass -File tools\ue\build_all.ps1` from a clean checkout ends with the
level saved.

## U8. One-click game launcher — ✅
**What:** double-clicking the `.uproject` opens the Unreal Editor (that is what a .uproject does), which confused
the first play attempt. `Play.bat` at the workspace root starts the game itself: `UnrealEditor.exe <uproject> -game`,
windowed 1920×1080, no editor UI; you spawn at the front door as a Defender. `-Fullscreen`, `-Width` / `-Height`,
`-AutoShot <s>` (dev: screenshot + quit) are passed through to `tools/ue/play.ps1`, which also checks that the game
module is compiled and the level exists before launching.
**How:** `Play.bat` → `tools/ue/play.ps1`.
**Test:** verified 2026-11-06 with `-AutoShot 25`: window opens, round 1 live at 2:18, 5 v 5, HUD and map drawn.

## U9. Renderer tuned for the GPU in this machine — ✅
**What:** the machine has a GeForce GTX 1660 (6 GB, no ray-tracing cores). With Lumen on, Unreal builds a mesh
distance field on the CPU for every re-imported mesh (≈ 13 s each, 25 min after a full reimport) and the card
renders Lumen's software lighting slowly. `DefaultEngine.ini` now disables Lumen GI / reflections and distance-field
generation and uses classic shadow maps, screen-space reflections and TAA; the sky light gives the ambient term.
Four commented lines in the ini switch Lumen back on for a stronger card.
**Test:** after the facade reimport the first launch dropped from ≈ 25 min of distance-field builds to 90 s
(`data/screenshot_facades.png`, 1920×1080).

---

## U10. Soldier + rifle models — ✅
**What:** replaces the engine-primitive stand-ins with generated low-poly models: a soldier (helmet, head,
vest over a team-coloured uniform, arms holding the rifle, legs; crouch handled by the capsule as before)
seen by the others, and a rifle (receiver, barrel, magazine, stock, sights, grip) in first and third person.
**How:** `tools/build_map.py` emits `soldier.obj` / `rifle.obj` with the same `MeshWriter` as the props
(slot 0 = uniform, tinted per team by `ApplyTeamVisuals`); `build_level.py` imports them like any mesh;
`AFPSCharacter` loads `/Game/Meshes/soldier` and `/Game/Meshes/rifle` and falls back to the primitives if
they are missing.
**Test:** screenshot of a bot; the 1P rifle sits bottom-right with the sights under the crosshair.
**Verified:** `data/screenshot_soldier.png` (a blue defender bot during warm-up, via `play.ps1 -AutoShot 3 -LookMate`)
and `data/screenshot_ground.png` (the 1P rifle). Gotcha: the ring cleaner dropped every box thinner than 5 cm
(barrel, grip, sights), so `MeshWriter.box` now passes `eps = min(side) / 4`; the 1P rifle sits at (38, 17, −24) cm,
scale 0.6, yaw −4° (the first placement was too close to the lens).

## U11. Sound — ✅ built
**What:** synthesised WAVs (numpy): rifle shot, dry fire, reload, footsteps, hit marker, headshot, death,
plant / defuse ticks, bomb beeps accelerating to the blast, explosion, round start / win / lose, match end.
**How:** `tools/make_sounds.py` → `assets/sounds/*.wav`; `build_level.py` imports them to `/Game/Sounds`;
the game loads them by path (`UGameplayStatics::PlaySoundAtLocation` / `PlaySound2D`), so a missing file is
silent rather than fatal.
**Test:** fire, reload, walk, plant the bomb: each has a sound; the bomb beeps faster as the timer runs out.

## U12. No depth of field + dev look-at switch — ✅
**What:** `r.DepthOfFieldQuality=0` so the far end of the street stays sharp; `-pbm_lookmate` turns the
auto-screenshot towards the nearest living teammate.
**How:** `Config/DefaultEngine.ini`, `BombGameMode.cpp` (autoshot block).
**Test:** `play.ps1 -AutoShot 3 -LookMate` → `data/screenshot_soldier.png`.

## U13. HUD readability — ✅
**What:** the user squints: the HUD used Unreal's 9–18 px engine fonts at scale 1. Every text is now sized in
pixels of a 1080p screen (small 22, body 26, big 44, huge 64, scaled with the window height) using one
distance-field font (crisp at any scale); panels widened to match. World callout labels enlarged too.
**How:** `BombHUD.*` (`Px()` helper: scale = wanted pixels / font height), `CalloutMarker.cpp` world size.
**Test:** 1920×1080 screenshot: the smallest text (controls reminder, kill feed) is ≥ 22 px tall.
**Verified:** `data/screenshot_hud.png` (freeze time of round 1). The world callout labels (`ACalloutMarker`) now turn
towards the camera every 0.1 s so they are never mirrored, and the HUD callout moves above the key bar during freeze.

## G1–G11. Game rules ported from FPS Reborn — ✅ built (G4 / G6 / G7 / G8 to playtest)
**Source:** `_CODE/fps-reborn` (`js/modes.js`, `weapons.js`, `grenades.js`, `drops.js`, `shop.js`, `game.js`).
Only the rules are taken; none of the art direction (no flowers, no bubbles, no cats): the guns are made-up
realistic firearms.
**What (per feature):**
* G1 roster (name → FPS Reborn source, price, slot): P9 pistol (Dandelion, free), SO-12 sawn-off (Pansy Pump,
  250), MP-11 machine pistol (Buzzy-11, 400), SP-7 suppressed pistol (Hush Daisy, 500), R-44 revolver (Cactus,
  800); SMG-9 (Tulip Popper, 900), PS-12 pump shotgun (Sunflower Splash, 1 800), AR-7 rifle (Bloom Blaster,
  2 900), AR-7S suppressed rifle (Hush Bloom, 2 900); SR-30 scout (Bluebell Scout, 950), LR-50 long rifle
  (Lily Longshot, 4 500); combat knife (free, 1.5× move speed). Stats copied 1:1: auto / semi, fire rate,
  magazine, reserve, reload time, range, head / body / legs damage, distance fall-off, spread (base + walk + run +
  air, crouch multiplier, per-shot bloom, max), recoil kick pattern then sustained + side wobble, silenced
  (quieter, no minimap ping — minimap not implemented), scope (right mouse, FOV 32 / 28, unscoped penalty).
* G2 buy menu: B opens it during the buy phase (closes when the round goes live or you die); three columns
  (sidearms / main guns / snipers) + kevlar + grenades, each card = name, price, Power / Speed / Range bars,
  greyed when unaffordable, "owned" highlight; left click buys, right click sells back anything bought this
  phase; header shows money, "next round at least …" and the countdown.
* G3 economy (`ECON`): start 800, win 3 000, loss 1 900 → 2 400 → 2 900 by consecutive losses (reset on a win),
  200 per kill, 300 to every attacker on a plant, cap 9 000. Players who died lose their loadout (survivors keep
  it, ammo refilled); halftime resets everyone to 800 + pistol; this-phase purchases refunded when replaced.
* G4 round flow: buy phase 20 s (round 1: 5 s) with free movement inside an invisible spawn fence, 100 s live,
  5 s result screen, first to 13 with win-by-two (overtime beyond 24 rounds swaps sides every round), sides
  swap after round 12, the round timer at 0:00 only ends the round once nobody is mid-plant.
* G5 kevlar: light (400, +25) / heavy (1 000, +50) absorbs damage before health; a lower tier bought over a
  higher one is refused.
* G6 grenades: smoke (200, max 2, 1.3 s fuse, 3.6 m cloud for 15 s), flash (250, max 2, 0.9 s fuse, blinds for
  up to 35 m when looked at), frag (400, max 1, 1.8 s fuse, 6 m radius, 130 max damage); they bounce; your own
  never hurt your team. Keys: Q flash, E smoke, C frag (quick-throw: left click lob, right click under-hand),
  5 cycles the ones you own.
* G7 drops: on death the main gun falls (ammo kept) and anyone can take it: F within 2.2 m (swapping the one
  held), bots walk over it within 14 m when they have no main gun; everything is cleared at round start.
* G8 bomb carry: 4 selects the bomb (hold to plant inside A / B; defenders hold 4 next to it to defuse); when
  the human attacks, the bomb waits on the ground in front of them at round start and bot teammates leave it
  alone until 10 s after GO, until the human leaves spawn without it, or dies; the blast kills everyone within
  10 m. Plant 4 s, fuse 45 s, defuse 7 s (unchanged).
* G9 bots buy with the ported `_botsBuy` (weighted picks by money, saving rounds, pistol-round sidearms,
  kevlar, attackers' smokes + flashes, occasional frag), throw a smoke and a flash when hitting a site, frag
  enemies seen 5–25 m away, stand still to "scope".
* G10 HUD: money (bottom-left, "+200" pop-up), weapon name + slot keys, kevlar, grenade pips, scoreboard
  money column, kill feed "killer [weapon] victim" with a headshot mark.
* G11 models: `tools/build_map.py` gains one generated mesh per gun / knife / grenade (box-built, realistic
  proportions: slides, barrels, drum / box magazines, pump, suppressor cans, scopes) used in first person and
  on the ground as drops.
**How:** new `Weapons.h/.cpp` (static table + `FGunState`), `Inventory` on `AFPSCharacter` (slots main / side /
knife / nade / bomb, kevlar, money moved to `ABombPlayerState`), `AGrenadeActor`, `AWeaponPickup`, buy menu drawn
by `ABombHUD` with hit boxes, economy + flow in `ABombGameMode`, bot buying / nade use in `ABotController`.
**Test:** B in the buy phase shows the menu; buying an AR-7 changes the 1P model and ammo; a kill shows +200;
losing three rounds in a row pays 2 900; a dropped gun can be picked up with F; a flash whites the screen.
**Verified 2026-11-06:** `play.ps1 -AutoShot 8 -Extra -pbm_openshop` → `data/screenshot_shop.png` (the menu, $800, "next round
at least $2 700", countdown); `-AutoShot 70` / `-AutoShot 165` run logs: round 1 bots bought MP-11 / R-44 / SP-7 + kevlar + smokes,
round 2 (attackers won) bots hold AR-7 / AR-7S / SMG-9 / PS-12 / SR-30 with kevlar 25–50 and 1–3 grenades, the human has $2 700
after the lost round. Gotcha: the bots' `BuyPhase()` must run after the phase is set to Freeze (the shop is closed otherwise).
Dev switch `-pbm_openshop` opens the buy menu 1.5 s before the auto screenshot.

## A1. Parisian building asset — ✅ built
**What:** the user-supplied *3 Piece Parisian Apartment Building* (Blender pack in Downloads › *Parisian Building
Blender Assets*: 231 mesh objects, 625 k vertices, 3 buildings parented to control empties, 28 materials whose
images mostly point at the author's machine, `Textures.zip` next to it) becomes 8 game pieces in
`assets/models/parisian_block/`: `block_center` (10.2 × 11.0 × 19.2 m), `block_end` (10.2 × 7.0 m), `block_corner`
(17.8 × 14.3 m), `ruin_center` / `ruin_end` / `ruin_corner` (the same shelled: roof and upper corners sheared off
along three slanted planes), `wall_chunk_a` / `wall_chunk_b` (a single surviving street facade, 1.3–1.8 m thick,
jagged top). Each FBX ships its textures in a `.fbm` folder; `pieces.json` lists sizes.
**How:** `tools/import_blender_assets.py` runs Blender 5.2 headless with `tools/blender/export_parisian.py`: unzips the
textures, unpacks the ones packed in the .blend, rebuilds every material as Principled BSDF + base-colour image (the
red terracotta wall texture is desaturated and brightened into `ParisStone_COL_4K.jpg`, Paris being cream limestone),
box-projects UVs on the wall-type parts (their materials used object-space mapping), decimates parts above 6 000
polygons to 35 %, clears the parent empties, joins each building, recentres it on its footprint, cuts the ruins and
slabs with `bmesh.ops.bisect_plane`, and exports FBX (−Y forward, Z up, textures copied). `build_level.py` imports
the FBX files through a second Interchange pipeline (`/Game/Pipelines/FbxPipeline`, materials + textures on) into
`/Game/Models/parisian_block/`, complex collision; `build_map.py` places instances from `layout.json › warzone.models`.
**Test:** `python -I tools/import_blender_assets.py` (≈ 25 s) prints the 8 pieces; Blender workbench previews show
textured Haussmann facades with mansard roofs and dormers. Gotchas: the parts are parented (clear the parent and keep
the world matrix before applying transforms, or the local cuts miss); `FParse::Value` stops at commas.
**Source:** the pack stays in Downloads (339 MB); only the exported pieces (≈ 60 MB) live in the workspace.

## A2. War-zone dressing — ✅ built
**What:** the neighbourhood stays recognisable (photo facades, real ground, street layout untouched) but reads as a
front line. `layout.json › warzone`:
* `models` — 7 A1 pieces: a shelled block in the west of Square Émile-Chautemps (site B), a corner ruin collapsed
  into Boulevard de Sébastopol south (a chokepoint), a ruin on Sébastopol north, a surviving facade along the
  boulevard, facade slabs at Mid plaza and on Rue N-D-de-Nazareth west of A, an intact block by the attackers' spawn.
* `props` — 53 placed: rubble mounds (3 variants, random boxes on a cone + beams), sandbag walls, Czech hedgehogs,
  jersey barriers, burnt-out cars (flat tyres, scorched ground), craters (scorch disc + debris ring), smoke columns
  (stacked drifting spheres over a fire glow, visible across the map).
* `mood` — low cold sun (pitch −24°, intensity 3.2, bluish white), stronger sky light, dusty haze (fog density 0.045,
  warm grey colour, from 15 m), saturation 0.78, contrast 1.06.
**How:** `build_map.py` generates the dressing meshes (`make_rubble`, `make_sandbags`, `make_hedgehog`, `make_jersey`,
`make_burnt_car`, `make_crater`, `make_smoke_column`) and snaps every model / prop to the nearest free spot
(`snap_spot`: spiral search up to 16 m; corners, centre and mid-edges outside every building with 1 m clearance,
outside the bomb-site boxes for models, 4 m clear of the spawns, and unless `may_block` is set at least 3 m of
carriageway left beside the nearest road axis); the footprints join `prop_rings` so spawn points avoid them.
Model actors carry `"model": "<pack>/<piece>"` in `level.json`; `build_level.py` resolves them from `/Game/Models/`
and applies `mood` to the sun, sky light, fog and post-process. Dev switch `-pbm_teleport=X,Y,Z,Yaw,Pitch` (UE cm /
degrees) places the human for a screenshot.
**Test:** `build_map.py` prints the snapped positions; `play.ps1 -AutoShot 7 -Extra -pbm_teleport=-8200,9800,180,151,-4`
shows the B site ruin, `-pbm_teleport=-18300,26200,180,-54,2` the Sébastopol chokepoint.
**Verified:** `data/screenshot_warzone_b.png`, `data/screenshot_warzone_sebasto.png`.

## Verification log

| Check | Result |
|---|---|
| UBT compile (Development Editor) | ✅ OK, no warnings |
| `build_level.py` | ✅ 669 meshes imported (16 min first run, cached after), level assembled in 4 s, nav mesh 4 096 tiles in 14 min, `L_SaintMartin.umap` 4.2 MB |
| Standalone `-game` run with `-pbm_autoshot=40` | ✅ Warmup → Freeze → Live, 5 v 5 alive, bots navigate to the sites / up the lanes, screenshot `unreal/ParisBombMap/Saved/Screenshots/WindowsEditor/autoshot.png` (copied to `data/screenshot_ingame.png`) |
| `build_facades.py` (3 shards) + `build_map.py` + level rebuild with the atlases | ✅ 232 buildings textured in 15 min; 264 materials, 669 meshes re-imported in 15 min, level assembled, no warnings; in-game screenshot shows the real shops along Rue Saint-Martin |
| Plant / defuse / explosion | 🟡 implemented, not yet observed in a run |
| Human playtest (gun feel, movement) | 🟡 pending |

## Known limitations

- Characters and the rifle are generated low-poly box models with no animation (they glide; crouch squashes them).
- Bullet tracers are debug lines (invisible in Shipping builds).
- Sounds are synthesised (no recorded samples); no multiplayer replication (single machine only).
- Photo facades bake in parked cars, pedestrians and the survey-day lighting; the orthophoto ground bakes in
  cars and tree canopies; untextured walls (courtyards, backs) keep the procedural facade.
- Callout labels now turn to face the camera (no longer mirrored).
- War zone: the ruins are plane cuts of the intact pack (no interior debris, open cut faces), the smoke columns are
  static stacked spheres, the facade slabs stand alone; the park's low-poly trees hide the B-site ruin from some angles.
- Game rules still to playtest by hand: win-by-two / overtime, grenade detonations, dropped-gun pickup, the human
  bomb hand-off (play as attacker with M).
