"""Assemble the Unreal level from data/map/level.json.  RUNS INSIDE THE UNREAL EDITOR (Python plugin).

    UnrealEditor-Cmd.exe ParisBombMap.uproject -run=pythonscript -script="<this file>"
    (wrapped by tools/ue/build_level.ps1)

Steps: import textures -> create materials -> import OBJ meshes (Interchange) -> new level ->
place meshes / props / blockers / spawns / sites / callouts / nav bounds / lights -> build nav -> save.
Idempotent: existing textures/materials/meshes are reused unless FORCE_REIMPORT is set.
"""
import json
import os
import sys
import time
import traceback

import unreal

FORCE_MATERIALS = os.environ.get("PBM_FORCE_MATERIALS", "0") == "1"
FORCE_REIMPORT = os.environ.get("PBM_FORCE_REIMPORT", "0") == "1"
SKIP_NAV = os.environ.get("PBM_SKIP_NAV", "0") == "1"

PROJECT_DIR = os.path.normpath(unreal.Paths.project_dir())
ROOT = os.path.normpath(os.path.join(PROJECT_DIR, "..", ".."))
LEVEL_JSON = os.path.join(ROOT, "data", "map", "level.json")
MESH_PKG = "/Game/Meshes"
TEX_PKG = "/Game/Textures"
MAT_PKG = "/Game/Materials"
MAP_PKG = "/Game/Maps"
LEVEL_NAME = "L_SaintMartin"
LEVEL_PATH = f"{MAP_PKG}/{LEVEL_NAME}"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
T0 = time.time()


def log(msg):
    unreal.log(f"[build_level +{time.time() - T0:6.1f}s] {msg}")


def warn(msg):
    unreal.log_warning(f"[build_level] {msg}")


def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def rot_yaw(yaw):
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw))


with open(LEVEL_JSON, "r", encoding="utf-8") as f:
    LEVEL = json.load(f)
log(f"level.json: {len(LEVEL['meshes'])} meshes, {len(LEVEL['actors'])} actors")

# ====================================================================== textures
TEX_DIR = os.path.join(ROOT, LEVEL["textures_dir"])
textures = {}


def stale(asset_pkg_path, source_file):
    """True when the asset is missing or its source file on disk is newer than the saved .uasset."""
    if FORCE_REIMPORT or not eal.does_asset_exist(asset_pkg_path):
        return True
    uasset = os.path.join(PROJECT_DIR, "Content", asset_pkg_path[len("/Game/"):] + ".uasset")
    try:
        return os.path.getmtime(source_file) > os.path.getmtime(uasset) + 1.0
    except OSError:
        return True


def import_texture(key):
    """key = file name in textures_dir ("facade.png") or a workspace-relative path ("assets/facades/x.jpg")."""
    src = os.path.join(ROOT, key) if "/" in key or "\\" in key else os.path.join(TEX_DIR, key)
    name = "T_" + os.path.splitext(os.path.basename(key))[0]
    path = f"{TEX_PKG}/{name}"
    if not stale(path, src):
        return unreal.load_asset(path)
    png_name = os.path.basename(key)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", TEX_PKG)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if tex is None:
        # some importers keep the file name: try that
        alt = f"{TEX_PKG}/{os.path.splitext(png_name)[0]}"
        tex = unreal.load_asset(alt)
        if tex is not None:
            eal.rename_asset(alt, path)
            tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError(f"texture import failed: {png_name}")
    return tex


def tex_key(spec):
    return spec.get("texture") or spec.get("texture_path")


for spec in LEVEL["materials"].values():
    for k in (tex_key(spec), spec.get("detail")):
        if k and k not in textures:
            textures[k] = import_texture(k)
if os.path.exists(os.path.join(TEX_DIR, "minimap.png")):
    textures["minimap.png"] = import_texture("minimap.png")          # U17 HUD minimap
log(f"textures: {len(textures)}")

# ====================================================================== materials
materials = {}


def make_material(name, spec):
    path = f"{MAT_PKG}/{name}"
    if eal.does_asset_exist(path) and not FORCE_REIMPORT and not FORCE_MATERIALS:
        return unreal.load_asset(path)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = asset_tools.create_asset(name, MAT_PKG, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", bool(spec.get("two_sided", True)))
    if tex_key(spec):
        ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
        ts.set_editor_property("texture", textures[tex_key(spec)])
        if spec.get("detail"):
            # base = ortho * (detail * 2): the aerial photo gives the colour, the tiling texture the grain
            tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 300)
            tiling = spec.get("detail_tiling", [50.0, 50.0])
            tc.set_editor_property("u_tiling", float(tiling[0])); tc.set_editor_property("v_tiling", float(tiling[1]))
            td = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 300)
            td.set_editor_property("texture", textures[spec["detail"]])
            mel.connect_material_expressions(tc, "", td, "UVs")
            # grain = 0.7 + 0.6 * detail  (mean ~1: the photo keeps its brightness, the detail only modulates it)
            k2 = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 500)
            k2.set_editor_property("r", 0.6)
            k3 = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 600)
            k3.set_editor_property("r", 0.7)
            m0 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -550, 350)
            mel.connect_material_expressions(td, "RGB", m0, "A"); mel.connect_material_expressions(k2, "", m0, "B")
            m1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -450, 350)
            mel.connect_material_expressions(m0, "", m1, "A"); mel.connect_material_expressions(k3, "", m1, "B")
            m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 100)
            mel.connect_material_expressions(ts, "RGB", m2, "A"); mel.connect_material_expressions(m1, "", m2, "B")
            mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        col = spec.get("color", [0.5, 0.5, 0.5])
        c.set_editor_property("constant", unreal.LinearColor(col[0], col[1], col[2], 1.0))
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
    r.set_editor_property("r", float(spec.get("roughness", 0.8)))
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if spec.get("metallic"):
        m = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 350)
        m.set_editor_property("r", float(spec["metallic"]))
        mel.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    if spec.get("emissive"):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 450)
        em = spec["emissive"]
        e.set_editor_property("constant", unreal.LinearColor(em[0], em[1], em[2], 1.0))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return mat


MATERIAL_SPECS = dict(LEVEL["materials"])
MATERIAL_SPECS["M_TeamAttackers"] = {"color": [0.95, 0.42, 0.05], "roughness": 0.7}
MATERIAL_SPECS["M_TeamDefenders"] = {"color": [0.12, 0.42, 0.95], "roughness": 0.7}
MATERIAL_SPECS["M_Bomb"] = {"color": [0.15, 0.12, 0.10], "roughness": 0.5, "metallic": 0.3, "emissive": [2.0, 0.05, 0.02]}
for name, spec in MATERIAL_SPECS.items():
    materials[name] = make_material(name, spec)
log(f"materials: {len(materials)}")

# ====================================================================== sounds (assets/sounds/*.wav -> /Game/Sounds)
SND_PKG = "/Game/Sounds"
SND_DIR = os.path.join(ROOT, "assets", "sounds")
n_snd = 0
if os.path.isdir(SND_DIR):
    for wav in sorted(os.listdir(SND_DIR)):
        if not wav.lower().endswith(".wav"):
            continue
        name = os.path.splitext(wav)[0]
        path = f"{SND_PKG}/{name}"
        if not stale(path, os.path.join(SND_DIR, wav)):
            n_snd += 1; continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SND_DIR, wav))
        task.set_editor_property("destination_path", SND_PKG)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])
        if eal.does_asset_exist(path):
            n_snd += 1
        else:
            warn(f"sound import failed: {wav}")
log(f"sounds: {n_snd}")

# ====================================================================== mesh import (Interchange, OBJ)
PIPE_PATH = "/Game/Pipelines/ObjPipeline"


def make_pipeline():
    if eal.does_asset_exist(PIPE_PATH):
        pipe = unreal.load_asset(PIPE_PATH)
    else:
        src = unreal.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline")
        pipe = asset_tools.duplicate_asset("ObjPipeline", "/Game/Pipelines", src)
    mp = pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property("collision", False)
    mp.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    try:
        mp.set_editor_property("generate_lightmap_u_vs", False)
    except Exception:
        try:
            mp.set_editor_property("generate_lightmap_uvs", False)
        except Exception as e:
            warn(f"lightmap uv option: {e}")
    try:
        mp.set_editor_property("build_nanite", False)
    except Exception:
        pass
    matp = pipe.get_editor_property("material_pipeline")
    matp.set_editor_property("import_materials", False)
    try:
        matp.get_editor_property("texture_pipeline").set_editor_property("import_textures", False)
    except Exception as e:
        warn(f"texture pipeline option: {e}")
    cm = pipe.get_editor_property("common_meshes_properties")
    cm.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    pipe.set_editor_property("use_source_name_for_asset", True)
    eal.save_asset(PIPE_PATH)
    return pipe


pipeline = make_pipeline()
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
meshes = {}


def import_mesh(mesh_id, rel_file):
    path = f"{MESH_PKG}/{mesh_id}"
    obj_path = os.path.join(ROOT, "data", "map", rel_file)
    if not stale(path, obj_path):
        return unreal.load_asset(path)
    source = unreal.InterchangeManager.create_source_data(obj_path)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipeline.get_path_name())])
    result = manager.import_asset(MESH_PKG, source, params)
    imported = None
    if isinstance(result, tuple):
        ok, objs = result[0], result[1]
        for o in objs:
            if isinstance(o, unreal.StaticMesh):
                imported = o
    if imported is None:
        if eal.does_asset_exist(path):
            imported = unreal.load_asset(path)
    if imported is None:
        raise RuntimeError(f"mesh import failed: {rel_file}")
    if imported.get_path_name().split(".")[0] != path:
        # imported under a different name (group name): move it
        eal.rename_asset(imported.get_path_name().split(".")[0], path)
        imported = unreal.load_asset(path)
    return imported


def assign_materials(mesh, slots):
    static_mats = list(mesh.get_editor_property("static_materials"))
    by_name = {}
    for i, sm in enumerate(static_mats):
        n = str(sm.get_editor_property("imported_material_slot_name"))
        if not n or n == "None":
            n = str(sm.get_editor_property("material_slot_name"))
        by_name[n] = i
    for i, sm in enumerate(static_mats):
        n = str(sm.get_editor_property("imported_material_slot_name"))
        if n not in materials:
            n = str(sm.get_editor_property("material_slot_name"))
        if n not in materials and i < len(slots):
            n = slots[i]
        if n in materials:
            mesh.set_material(i, materials[n])
        else:
            warn(f"{mesh.get_name()}: no material for slot '{n}'")


t_import = time.time()
total = len(LEVEL["meshes"])
for k, (mesh_id, info) in enumerate(LEVEL["meshes"].items()):
    try:
        was_new = stale(f"{MESH_PKG}/{mesh_id}", os.path.join(ROOT, "data", "map", info["file"]))
        mesh = import_mesh(mesh_id, info["file"])
        if was_new:
            assign_materials(mesh, info["slots"])
            unreal.MapBuilderLibrary.set_complex_collision(mesh)
            eal.save_asset(f"{MESH_PKG}/{mesh_id}", only_if_is_dirty=False)
        meshes[mesh_id] = mesh
    except Exception as e:
        warn(f"mesh {mesh_id}: {e}\n{traceback.format_exc()}")
    if k % 50 == 0:
        log(f"meshes {k}/{total}")
log(f"meshes imported: {len(meshes)}/{total} in {time.time() - t_import:.0f}s")

# ====================================================================== user-supplied models (FBX with textures -> /Game/Models/<pack>)
FBX_PIPE_PATH = "/Game/Pipelines/FbxPipeline"
MODELS_DIR = os.path.join(ROOT, "assets", "models")
models = {}


def make_fbx_pipeline():
    if eal.does_asset_exist(FBX_PIPE_PATH):
        pipe = unreal.load_asset(FBX_PIPE_PATH)
    else:
        src = unreal.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline")
        pipe = asset_tools.duplicate_asset("FbxPipeline", "/Game/Pipelines", src)
    mp = pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property("collision", False)
    mp.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    for prop in ("generate_lightmap_u_vs", "generate_lightmap_uvs"):
        try:
            mp.set_editor_property(prop, False); break
        except Exception:
            pass
    try:
        mp.set_editor_property("build_nanite", False)
    except Exception:
        pass
    matp = pipe.get_editor_property("material_pipeline")
    matp.set_editor_property("import_materials", True)
    try:
        matp.get_editor_property("texture_pipeline").set_editor_property("import_textures", True)
    except Exception as e:
        warn(f"fbx texture pipeline option: {e}")
    cm = pipe.get_editor_property("common_meshes_properties")
    cm.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    pipe.set_editor_property("use_source_name_for_asset", True)
    eal.save_asset(FBX_PIPE_PATH)
    return pipe


def import_model(pack, key, fbx_file):
    pkg = f"/Game/Models/{pack}"
    path = f"{pkg}/{key}"
    if not stale(path, fbx_file):
        return unreal.load_asset(path)
    source = unreal.InterchangeManager.create_source_data(fbx_file)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(fbx_pipeline.get_path_name())])
    result = manager.import_asset(pkg, source, params)
    imported = None
    if isinstance(result, tuple):
        for o in result[1]:
            if isinstance(o, unreal.StaticMesh):
                imported = o
    if imported is None and eal.does_asset_exist(path):
        imported = unreal.load_asset(path)
    if imported is None:
        raise RuntimeError(f"model import failed: {fbx_file}")
    if imported.get_path_name().split(".")[0] != path:
        eal.rename_asset(imported.get_path_name().split(".")[0], path)
        imported = unreal.load_asset(path)
    unreal.MapBuilderLibrary.set_complex_collision(imported)
    eal.save_directory(pkg, only_if_is_dirty=True, recursive=True)
    return imported


if os.path.isdir(MODELS_DIR):
    fbx_pipeline = None
    for pack in sorted(os.listdir(MODELS_DIR)):
        pj = os.path.join(MODELS_DIR, pack, "pieces.json")
        if not os.path.isfile(pj):
            continue
        with open(pj, "r", encoding="utf-8") as f:
            pieces = json.load(f)["pieces"]
        for key, info in pieces.items():
            try:
                if fbx_pipeline is None:
                    fbx_pipeline = make_fbx_pipeline()
                t0 = time.time()
                was_new = stale(f"/Game/Models/{pack}/{key}", os.path.join(MODELS_DIR, pack, info["fbx"]))
                models[f"{pack}/{key}"] = import_model(pack, key, os.path.join(MODELS_DIR, pack, info["fbx"]))
                if was_new:
                    log(f"model {pack}/{key} imported in {time.time() - t0:.0f}s")
            except Exception as e:
                warn(f"model {pack}/{key}: {e}\n{traceback.format_exc()}")
log(f"models: {len(models)}")

# ====================================================================== level
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Start from a blank untitled map and save it over LEVEL_PATH at the end (NewLevel refuses to
# overwrite a map that was loaded at start-up, even after it has been force-deleted).
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if world is None:
    raise RuntimeError("could not create a blank map")
if eal.does_asset_exist(LEVEL_PATH):
    eal.delete_asset(LEVEL_PATH)
umap = os.path.join(PROJECT_DIR, "Content", "Maps", LEVEL_NAME + ".umap")
if os.path.exists(umap):
    try:
        os.remove(umap)
    except OSError as e:
        warn(f"could not remove {umap}: {e}")
log(f"blank map created; will be saved as {LEVEL_PATH}")


def save_level():
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    if not ok:
        raise RuntimeError(f"could not save {LEVEL_PATH}")

placed = 0
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def spawn_class(cls, loc, rot):
    """Spawn an actor; the EditorActorSubsystem path can return None under the commandlet, so fall back."""
    actor = None
    try:
        actor = eas.spawn_actor_from_class(cls, loc, rot)
    except Exception as e:
        warn(f"spawn_class({cls}): {e}")
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    return actor


def spawn_mesh_actor(mesh, loc, rot):
    actor = spawn_class(unreal.StaticMeshActor, loc, rot)
    if actor is None:
        return None
    smc = actor.get_editor_property("static_mesh_component")
    smc.set_static_mesh(mesh)
    return actor


for a in LEVEL["actors"]:
    mesh = models.get(a["model"]) if "model" in a else meshes.get(a.get("mesh"))
    if mesh is None:
        if "model" in a:
            warn(f"model {a['model']} missing for {a['name']}")
        continue
    actor = spawn_mesh_actor(mesh, vec(a["loc"]), rot_yaw(a["yaw"]))
    if actor is None:
        warn(f"could not spawn {a['name']}")
        continue
    actor.set_actor_label(a["name"])
    actor.set_folder_path(unreal.Name(a["tags"][0] if a["tags"] else "misc"))
    actor.set_editor_property("tags", [unreal.Name(t) for t in a["tags"]])
    # U14: small things stop being drawn far away, tiny things cast no shadow (fewer draw calls, same look)
    tags = set(a["tags"])
    smc = actor.get_editor_property("static_mesh_component")
    if "tree" in tags:
        smc.set_editor_property("ld_max_draw_distance", 32000.0)
    elif "barrier" in tags or a.get("mesh") in ("crater", "sandbags", "hedgehog", "jersey", "bench", "planter"):
        smc.set_editor_property("ld_max_draw_distance", 15000.0)
        if a.get("mesh") in ("crater", "barrier"):
            smc.set_editor_property("cast_shadow", False)
    elif "prop" in tags:
        smc.set_editor_property("ld_max_draw_distance", 22000.0)
    placed += 1
    if placed % 200 == 0:
        log(f"placed {placed} actors")
log(f"placed {placed} mesh actors")

# invisible blocking walls (zone perimeter + street cut-offs)
for b in LEVEL["blockers"]:
    actor = spawn_mesh_actor(cube, vec(b["loc"]), rot_yaw(b["yaw"]))
    if actor is None:
        warn(f"could not spawn blocker {b['name']}")
        continue
    e = b["extent"]
    actor.set_actor_scale3d(unreal.Vector(e[0] / 50.0, e[1] / 50.0, e[2] / 50.0))
    actor.set_actor_label(b["name"])
    actor.set_folder_path(unreal.Name("blockers"))
    actor.set_actor_hidden_in_game(True)
    actor.set_editor_property("tags", [unreal.Name("blocker")])
    smc = actor.get_component_by_class(unreal.StaticMeshComponent)
    if smc:
        smc.set_collision_profile_name("BlockAll")
        smc.set_editor_property("cast_shadow", False)
log(f"blockers: {len(LEVEL['blockers'])}")

# player starts
for i, ps in enumerate(LEVEL["player_starts"]):
    actor = spawn_class(unreal.TeamPlayerStart, vec(ps["loc"]), rot_yaw(ps["yaw"]))
    actor.set_editor_property("team_id", int(ps["team"]))
    actor.set_editor_property("spawn_label", ps["label"])
    actor.set_actor_label(f"Start_{'T' if ps['team'] == 0 else 'CT'}_{i}")
    actor.set_folder_path(unreal.Name("gameplay"))

# bomb sites
for s in LEVEL["bomb_sites"]:
    actor = spawn_class(unreal.BombSite, vec(s["loc"]), rot_yaw(0))
    actor.set_editor_property("site_id", s["id"])
    actor.set_editor_property("label", s["label"])
    actor.set_extent(vec(s["extent"]))
    actor.set_actor_label(f"BombSite_{s['id']}")
    actor.set_folder_path(unreal.Name("gameplay"))

# callouts
for c in LEVEL["callouts"]:
    actor = spawn_class(unreal.CalloutMarker, vec(c["loc"]), rot_yaw(90))
    actor.set_callout(c["text"])
    actor.set_actor_label(f"Callout_{c['text']}")
    actor.set_folder_path(unreal.Name("gameplay"))
log("gameplay actors placed")

# nav mesh bounds
nb = LEVEL["nav_bounds"]
nav_vol = unreal.MapBuilderLibrary.spawn_box_volume(world, unreal.NavMeshBoundsVolume, vec(nb["loc"]), vec(nb["extent"]))
if nav_vol is None:
    warn("nav bounds volume could not be created")
else:
    nav_vol.set_actor_label("NavBounds")

# lighting & atmosphere
sun = spawn_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), unreal.Rotator(roll=0.0, pitch=float(LEVEL["sun"]["pitch"]), yaw=float(LEVEL["sun"]["yaw"])))
sun.set_actor_label("Sun")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
MOOD = LEVEL.get("mood") or {}
sc.set_intensity(float(MOOD.get("sun_intensity", 7.0)))
sc.set_editor_property("atmosphere_sun_light", True)
_sun_col = MOOD.get("sun_color", [255, 244, 224])
sc.set_editor_property("light_color", unreal.Color(int(_sun_col[0]), int(_sun_col[1]), int(_sun_col[2]), 255))
try:
    sc.set_editor_property("dynamic_shadow_distance_movable_light", 30000.0)
    sc.set_editor_property("dynamic_shadow_cascades", 4)
    sc.set_editor_property("cascade_distribution_exponent", 3.2)
    sc.set_editor_property("contact_shadow_length", 0.04)
    sc.set_editor_property("shadow_bias", 0.4)
    sc.set_editor_property("shadow_slope_bias", 0.4)
except Exception as e:
    warn(f"sun shadow settings: {e}")
sky = spawn_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), rot_yaw(0))
sky.set_actor_label("SkyAtmosphere")
skylight = spawn_class(unreal.SkyLight, unreal.Vector(0, 0, 3000), rot_yaw(0))
skylight.set_actor_label("SkyLight")
slc = skylight.get_component_by_class(unreal.SkyLightComponent)
slc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
slc.set_editor_property("real_time_capture", True)
slc.set_intensity(float(MOOD.get("skylight", 1.0)))
fog = spawn_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), rot_yaw(0))
fog.set_actor_label("Fog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", float(MOOD.get("fog_density", 0.012)))
fc.set_editor_property("fog_height_falloff", float(MOOD.get("fog_falloff", 0.35)))
fc.set_editor_property("start_distance", float(MOOD.get("fog_start", 3000.0)))
if MOOD.get("fog_color"):
    _fcol = MOOD["fog_color"]
    for prop in ("fog_inscattering_luminance", "fog_inscattering_color"):
        try:
            fc.set_editor_property(prop, unreal.LinearColor(_fcol[0], _fcol[1], _fcol[2], 1.0)); break
        except Exception:
            pass
try:
    clouds = spawn_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), rot_yaw(0))
    clouds.set_actor_label("Clouds")
except Exception as e:
    warn(f"clouds: {e}")
try:
    pp = spawn_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), rot_yaw(0))
    pp.set_actor_label("PostProcess")
    pp.set_editor_property("unbound", True)
    settings = pp.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 0.6)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 2.5)
    if MOOD.get("saturation") is not None:
        settings.set_editor_property("override_color_saturation", True)
        _sat = float(MOOD["saturation"])
        settings.set_editor_property("color_saturation", unreal.Vector4(_sat, _sat, _sat, 1.0))
    if MOOD.get("contrast") is not None:
        settings.set_editor_property("override_color_contrast", True)
        _con = float(MOOD["contrast"])
        settings.set_editor_property("color_contrast", unreal.Vector4(_con, _con, _con, 1.0))
    # U14 look: bloom, vignette, ambient occlusion, split-tone grading
    for key, prop, kind in (("bloom", "bloom_intensity", "f"), ("vignette", "vignette_intensity", "f"),
                            ("ao_intensity", "ambient_occlusion_intensity", "f"), ("ao_radius", "ambient_occlusion_radius", "f"),
                            ("shadows_tint", "color_gain_shadows", "v4"), ("highlights_tint", "color_gain_highlights", "v4")):
        if MOOD.get(key) is None:
            continue
        try:
            settings.set_editor_property("override_" + prop, True)
            v = MOOD[key]
            settings.set_editor_property(prop, float(v) if kind == "f" else unreal.Vector4(v[0], v[1], v[2], 1.0))
        except Exception as e:
            warn(f"post process {prop}: {e}")
    try:
        settings.set_editor_property("override_motion_blur_amount", True)
        settings.set_editor_property("motion_blur_amount", 0.0)
        settings.set_editor_property("override_film_grain_intensity", True)
        settings.set_editor_property("film_grain_intensity", 0.0)
    except Exception as e:
        warn(f"post process extras: {e}")
    pp.set_editor_property("settings", settings)
except Exception as e:
    warn(f"post process: {e}")
log("lighting placed")

# a warm light over the player's front door
home = LEVEL["home"]
door_light = spawn_class(unreal.PointLight, unreal.Vector(home["loc"][0], home["loc"][1], 350.0), rot_yaw(0))
door_light.set_actor_label("HomeDoorLight")
dl = door_light.get_component_by_class(unreal.PointLightComponent)
dl.set_intensity(3000.0)
dl.set_editor_property("light_color", unreal.Color(255, 200, 140, 255))
dl.set_editor_property("attenuation_radius", 1200.0)

save_level()
log("level saved (pre-nav)")

if not SKIP_NAV:
    try:
        ok = unreal.MapBuilderLibrary.build_navigation(world)
        log(f"navigation built: {ok}")
    except Exception as e:
        warn(f"nav build: {e}")
    save_level()

eal.save_directory("/Game", only_if_is_dirty=True, recursive=True)
log(f"DONE  level {LEVEL_PATH}  ({placed} mesh actors)")
