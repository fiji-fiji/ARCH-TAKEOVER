"""Runs INSIDE Blender (headless). Picks the guns of the roster out of the Quaternius "Ultimate Gun Pack" (CC0),
normalises them for the game and exports one FBX per roster mesh name:

    units -> metres (the pack is modelled ~6x life size), +X forward (as the pack), barrel axis on the origin
    (y centred, z = muzzle height, x = the pack's grip origin), transforms applied, one object per gun;
    the two suppressed guns get a pack silencer welded on the muzzle; the knife is the pack bayonet.

The pack's material colours are almost black (0.02-0.13) so they are lifted here, and the FBX material names are
kept so build_level.py can swap them for the map's own M_* materials (pieces.json > materials).

Usage (via tools/import_gun_pack.py):  blender -b -P tools/blender/export_guns.py -- <pack FBX dir> <out_dir>
"""
import bpy, os, sys, json
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SRC, OUT = argv[0], argv[1]
os.makedirs(OUT, exist_ok=True)


def log(*a):
    print("[export_guns]", *a, flush=True)


# roster mesh -> (pack model, metres per pack unit, silencer model | None)
GUNS = {
    "gun_p9": ("Pistol_1", 0.12, None),
    "gun_sp7": ("Pistol_3", 0.12, "Accessories/Silencer_2"),
    "gun_mp11": ("SubmachineGun_4", 0.14, None),
    "gun_r44": ("Revolver_2", 0.14, None),
    "gun_so12": ("Shotgun_SawedOff", 0.17, None),
    "gun_smg9": ("SubmachineGun_2", 0.17, None),
    "gun_ps12": ("Shotgun_1", 0.17, None),
    "gun_ar7": ("AssaultRifle_2", 0.17, None),
    "gun_ar7s": ("AssaultRifle_5", 0.17, "Accessories/Silencer_1"),
    "gun_sr30": ("SniperRifle_4", 0.16, None),
    "gun_lr50": ("SniperRifle_5", 0.16, None),
    "gun_knife": ("Accessories/Bayonet", 0.25, None),
}

# pack material name -> (base colour, roughness, metallic, map material that replaces it in Unreal)
PALETTE = {
    "Black": ((0.06, 0.06, 0.065), 0.55, 0.6, "M_GunBlack"),
    "Black2": ((0.04, 0.04, 0.045), 0.6, 0.5, "M_GunBlack"),
    "DarkMetal": ((0.16, 0.17, 0.18), 0.5, 0.8, "M_DarkMetal"),
    "Metal": ((0.35, 0.36, 0.38), 0.45, 0.8, "M_Metal"),
    "LightMetal": ((0.55, 0.56, 0.58), 0.35, 0.9, "M_LightMetal"),
    "LightMetal2": ((0.70, 0.70, 0.72), 0.3, 0.9, "M_LightMetal"),
    "Grey": ((0.22, 0.22, 0.23), 0.6, 0.3, "M_GunGrey"),
    "Wood": ((0.45, 0.30, 0.16), 0.8, 0.0, "M_Wood"),
    "DarkWood": ((0.28, 0.18, 0.11), 0.8, 0.0, "M_DarkWood"),
    "Main": ((0.20, 0.21, 0.22), 0.6, 0.3, "M_GunGrey"),
    "MainDark": ((0.08, 0.08, 0.09), 0.6, 0.4, "M_GunBlack"),
    "MainLight": ((0.30, 0.31, 0.32), 0.5, 0.5, "M_DarkMetal"),
    "Green": ((0.22, 0.28, 0.19), 0.7, 0.0, "M_GunGreen"),
    "Glass": ((0.10, 0.14, 0.18), 0.1, 0.6, "M_Glass"),
}


def load(model):
    """Import one pack FBX, apply its transforms, join its parts: returns the single mesh object."""
    path = os.path.join(SRC, model.replace("/", os.sep) + ".fbx")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    objs = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(objs) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = os.path.basename(model)
    return obj


def bounds(obj):
    xs = [v.co.x for v in obj.data.vertices]; ys = [v.co.y for v in obj.data.vertices]; zs = [v.co.z for v in obj.data.vertices]
    return Vector((min(xs), min(ys), min(zs))), Vector((max(xs), max(ys), max(zs)))


def muzzle(obj):
    """Centre of the front 6% of the model: where the barrel ends."""
    mn, mx = bounds(obj)
    cut = mx.x - 0.06 * (mx.x - mn.x)
    front = [v.co for v in obj.data.vertices if v.co.x >= cut]
    return Vector((mx.x, sum(v.y for v in front) / len(front), sum(v.z for v in front) / len(front)))


def restyle(obj):
    for slot in obj.material_slots:
        m = slot.material
        if not m:
            continue
        base = m.name.split(".")[0]
        col, rough, metal, _ = PALETTE.get(base, ((0.3, 0.3, 0.3), 0.6, 0.3, None))
        m.name = base
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
        nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
        bsdf.inputs["Base Color"].default_value = (*col, 1.0)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
        m.diffuse_color = (*col, 1.0)


def export(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
                             object_types={"MESH"}, mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             path_mode="RELATIVE", embed_textures=False, axis_forward="-Y", axis_up="Z", bake_space_transform=True)
    mn, mx = bounds(obj)
    size = [round(mx[i] - mn[i], 3) for i in range(3)]
    log("exported", name, "size", size, "polys", len(obj.data.polygons))
    return {"fbx": name + ".fbx", "size": size, "polys": len(obj.data.polygons)}


pieces = {}
for key, (model, k, silencer) in GUNS.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    gun = load(model)
    if silencer:
        mz = muzzle(gun)
        sil = load(silencer)
        smn, smx = bounds(sil)
        # the silencer's axis runs along +x; its back end is pushed a little over the muzzle
        overlap = 0.08 * (smx.x - smn.x)
        sil.location = Vector((mz.x - smn.x - overlap, mz.y - (smn.y + smx.y) / 2, mz.z - (smn.z + smx.z) / 2))
        bpy.ops.object.select_all(action="DESELECT")
        sil.select_set(True); gun.select_set(True)
        bpy.context.view_layer.objects.active = gun
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        bpy.ops.object.join()
        gun = bpy.context.view_layer.objects.active
    mn, mx = bounds(gun)
    if key == "gun_knife":
        centre = Vector((0.0, (mn.y + mx.y) / 2, (mn.z + mx.z) / 2))
    else:
        mz = muzzle(gun)
        centre = Vector((0.0, (mn.y + mx.y) / 2, mz.z))
    for v in gun.data.vertices:
        v.co = (v.co - centre) * k
    gun.location = (0, 0, 0)
    restyle(gun)
    pieces[key] = export(gun, key)
    pieces[key]["source"] = model + (" + " + silencer if silencer else "")

materials = {name: spec[3] for name, spec in PALETTE.items()}
with open(os.path.join(OUT, "pieces.json"), "w", encoding="utf-8") as f:
    json.dump({"source": "Quaternius Ultimate Gun Pack (CC0, https://quaternius.com)", "materials": materials, "pieces": pieces}, f, indent=1)
log("DONE", list(pieces))
