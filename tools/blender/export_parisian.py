"""Runs INSIDE Blender (headless). Splits the user-supplied "3 Piece Parisian Apartment Building" pack into game
pieces and exports them as FBX with their textures:

    block_center / block_end / block_corner   the three intact buildings (origin: footprint centre, z = 0)
    ruin_center / ruin_end / ruin_corner      the same buildings shelled: upper corners cut away, no roof on one
    wall_chunk_a / wall_chunk_b               a single surviving facade wall, jagged top

Usage (via tools/import_blender_assets.py):
    blender -b "<pack>.blend" -P tools/blender/export_parisian.py -- <textures_zip> <out_dir>

The pack's materials are complex node trees whose images mostly point to the author's machine; they are rebuilt here
as plain Principled BSDF + base-colour image (textures matched by file name from Textures.zip or unpacked from the
.blend), so the FBX carries textures that Unreal's Interchange importer understands. Wall-type objects get a box UV
projection (their materials used object-space mapping). Heavy objects are decimated.
"""
import bpy, bmesh, os, sys, zipfile, json, re, math, shutil
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TEX_ZIP, OUT = argv[0], argv[1]
TEX_SRC = os.path.join(OUT, "_textures_src")      # unzipped / unpacked originals (not committed)
TEX_DIR = os.path.join(OUT, "textures")           # the textures the FBX files reference (committed)
os.makedirs(OUT, exist_ok=True)
os.makedirs(TEX_SRC, exist_ok=True)
os.makedirs(TEX_DIR, exist_ok=True)


def log(*a):
    print("[export_parisian]", *a, flush=True)


# ---------------------------------------------------------------- textures
if not os.listdir(TEX_SRC):
    with zipfile.ZipFile(TEX_ZIP) as z:
        for m in z.namelist():
            base = os.path.basename(m)
            if not base or base.startswith("._") or base == ".DS_Store" or "__MACOSX" in m:
                continue
            with z.open(m) as src, open(os.path.join(TEX_SRC, base), "wb") as dst:
                shutil.copyfileobj(src, dst)
available = {f.lower(): os.path.join(TEX_SRC, f) for f in os.listdir(TEX_SRC)}


def unpack_image(img):
    """Packed images in the .blend -> a file in TEX_SRC. Returns the path or None."""
    if not img.packed_file:
        return None
    name = re.sub(r"\.\d+$", "", img.name)
    if not os.path.splitext(name)[1]:
        name += ".png"
    path = os.path.join(TEX_SRC, name)
    if not os.path.exists(path):
        try:
            img.filepath_raw = path
            img.save()
        except Exception as e:
            log("unpack failed", img.name, e)
            return None
    available[name.lower()] = path
    return path


for img in bpy.data.images:
    unpack_image(img)


def derive_stone(src_name, dst_name):
    """The pack's wall texture is a red-brown terracotta; Paris is cream limestone: desaturate + brighten a copy."""
    import numpy as np
    src = available.get(src_name.lower())
    dst = os.path.join(TEX_SRC, dst_name)
    if not src:
        return
    if not os.path.exists(dst):
        img = bpy.data.images.load(src, check_existing=True)
        w, h = img.size
        arr = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(arr)
        px = arr.reshape(-1, 4)
        lum = px[:, 0] * 0.3 + px[:, 1] * 0.59 + px[:, 2] * 0.11
        for ch, tint in enumerate((1.08, 1.02, 0.92)):
            px[:, ch] = np.clip((px[:, ch] * 0.3 + lum * 0.7) * 1.45 * tint + 0.08, 0.0, 1.0)
        out_img = bpy.data.images.new(dst_name, w, h)
        out_img.pixels.foreach_set(px.reshape(-1))
        out_img.filepath_raw = dst
        out_img.file_format = "JPEG"
        out_img.save()
    available[dst_name.lower()] = dst


derive_stone("TerracottaClay001_COL_4K.jpg", "ParisStone_COL_4K.jpg")


def tex(name):
    p = available.get(name.lower())
    if not p:
        log("MISSING texture", name)
    return p


# material name prefix -> (base colour texture | None, colour, roughness, metallic, uv tiling metres (None = keep UVs))
MATS = [
    ("Terracotta Clay", "ParisStone_COL_4K.jpg", (0.8, 0.74, 0.62), 0.85, 0.0, 6.0),
    ("Scratched Metal Tiles", "il_fullxfull.1215224480_f4c0_auto_x2.jpg", (0.45, 0.47, 0.5), 0.5, 0.6, 3.0),
    ("Painted Plaster", "PaintedPlaster018_2K_Color.jpg", (0.8, 0.76, 0.68), 0.8, 0.0, 4.0),
    ("Wooden surface", "WoodenSurface1_basecolor.jpg", (0.4, 0.28, 0.16), 0.7, 0.0, None),
    ("Wood.001", "wood-90_diffuse.jpg", (0.35, 0.24, 0.14), 0.7, 0.0, None),
    ("Wood", "wood-90_diffuse.jpg", (0.35, 0.24, 0.14), 0.7, 0.0, None),
    ("Window Curtains", "Window texture.png", (0.6, 0.6, 0.6), 0.5, 0.0, None),
    ("Material.004", "Lower winder texture.png", (0.5, 0.5, 0.5), 0.6, 0.0, None),
    ("Window FLAT", None, (0.05, 0.07, 0.09), 0.15, 0.6, None),
    ("Light Green Paint", None, (0.30, 0.40, 0.28), 0.6, 0.0, None),
    ("Rusty metal", None, (0.32, 0.18, 0.10), 0.8, 0.3, None),
    ("Material.001", None, (0.10, 0.10, 0.11), 0.5, 0.6, None),
    ("Material.002", None, (0.12, 0.10, 0.09), 0.6, 0.2, None),
    ("Tile roof", "Substance_graph_basecolor.jpg", (0.5, 0.3, 0.2), 0.8, 0.0, 3.0),
    ("Peeling Paint", "4K-plaster_17-diffuse.jpg", (0.7, 0.68, 0.6), 0.9, 0.0, 4.0),
    ("Bricks", "BricksRedUsed02_BaseColor2K.tga", (0.5, 0.25, 0.2), 0.9, 0.0, 3.0),
    ("Dots Stroke", None, (0.3, 0.3, 0.3), 0.8, 0.0, None),
]


def spec_for(mat_name):
    for prefix, t, col, rough, metal, tile in MATS:
        if mat_name.startswith(prefix):
            return t, col, rough, metal, tile
    return None, (0.5, 0.5, 0.5), 0.8, 0.0, None


tiling_of = {}
for mat in bpy.data.materials:
    t, col, rough, metal, tile = spec_for(mat.name)
    tiling_of[mat.name] = tile
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (*col, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    path = tex(t) if t else None
    if path:
        # one shared textures/ folder next to the FBX files (the FBX stores relative paths; no per-file .fbm copies)
        shared = os.path.join(TEX_DIR, os.path.basename(path))
        if not os.path.exists(shared):
            shutil.copyfile(path, shared)
        img = bpy.data.images.load(shared, check_existing=True)
        ti = nt.nodes.new("ShaderNodeTexImage")
        ti.image = img
        nt.links.new(ti.outputs["Color"], bsdf.inputs["Base Color"])
    mat.blend_method = "OPAQUE"
log("materials rebuilt:", len(bpy.data.materials))

# ---------------------------------------------------------------- objects per building
BUILDINGS = {"center": "Center Building", "end": "End Building", "corner": "Corner Building"}
WALL_LIKE = ("Main Walls", "End Wall", "Lower Facade", "Roof Support", "Cube", "Plane")


def objects_of(coll_name):
    coll = bpy.data.collections.get(coll_name)
    if not coll:
        return []
    out = []
    for o in coll.all_objects:
        if o.type == "MESH" and len(o.data.polygons) > 0:
            out.append(o)
    return out


def box_uv(obj, size):
    """Box projection in object space, one tile every `size` metres."""
    me = obj.data
    if not me.uv_layers:
        me.uv_layers.new(name="UVMap")
    uv = me.uv_layers.active.data
    mw = obj.matrix_world
    for poly in me.polygons:
        n = (mw.to_3x3() @ poly.normal).normalized()
        ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
        for li in poly.loop_indices:
            v = mw @ me.vertices[me.loops[li].vertex_index].co
            if az >= ax and az >= ay:
                u, w = v.x, v.y
            elif ax >= ay:
                u, w = v.y, v.z
            else:
                u, w = v.x, v.z
            uv[li].uv = (u / size, w / size)


def prepare(obj):
    """Apply UV projection per material tiling and decimate the heavy parts (on a copy)."""
    base = re.sub(r"\.\d+$", "", obj.name)
    tiles = [tiling_of.get(m.name) for m in obj.data.materials if m]
    if base in WALL_LIKE or (tiles and all(t for t in tiles)):
        size = min([t for t in tiles if t] or [4.0])
        box_uv(obj, size)
    if len(obj.data.polygons) > 6000:
        mod = obj.modifiers.new("dec", "DECIMATE")
        mod.ratio = 0.35
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj]):
            try:
                bpy.ops.object.modifier_apply(modifier=mod.name)
            except Exception as e:
                log("decimate failed", obj.name, e)


def join_copy(objs, name):
    """Duplicate + join into one mesh object, transforms applied, origin at the footprint centre on the ground."""
    bpy.ops.object.select_all(action="DESELECT")
    copies = []
    for o in objs:
        c = o.copy()
        c.data = o.data.copy()
        c.animation_data_clear()
        mw = o.matrix_world.copy()
        bpy.context.scene.collection.objects.link(c)
        c.parent = None                      # the pack parents every part to a control empty: keep the world placement
        c.matrix_world = mw
        copies.append(c)
    for c in copies:
        c.select_set(True)
    bpy.context.view_layer.objects.active = copies[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    # recentre
    bb = [joined.matrix_world @ Vector(c) for c in joined.bound_box]
    minv = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    maxv = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    centre = Vector(((minv.x + maxv.x) / 2, (minv.y + maxv.y) / 2, minv.z))
    for v in joined.data.vertices:
        v.co -= centre
    joined.location = (0, 0, 0)
    return joined, (maxv - minv)


def bisect(obj, co, no):
    """Cut the mesh by a plane and delete everything on the normal side."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-4, plane_co=co, plane_no=no, clear_outer=True, clear_inner=False)
    bm.to_mesh(obj.data)
    bm.free()


def export(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
                             object_types={"MESH"}, mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             path_mode="RELATIVE", embed_textures=False, axis_forward="-Y", axis_up="Z", bake_space_transform=True)
    bb = [Vector(c) for c in obj.bound_box]
    size = [round(max(v[i] for v in bb) - min(v[i] for v in bb), 2) for i in range(3)]
    log("exported", name, "size", size, "polys", len(obj.data.polygons))
    return {"fbx": name + ".fbx", "size": size, "polys": len(obj.data.polygons)}


pieces = {}
for key, coll in BUILDINGS.items():
    objs = objects_of(coll)
    if not objs:
        log("no objects in", coll)
        continue
    for o in objs:
        prepare(o)
    whole, size = join_copy(objs, f"block_{key}")
    pieces[f"block_{key}"] = export(whole, f"block_{key}")
    sx, sy, sz = size.x, size.y, size.z
    # ruin: the roof and the upper corners sheared off along two slanted planes, one wall opened
    ruin = whole.copy(); ruin.data = whole.data.copy(); bpy.context.scene.collection.objects.link(ruin); ruin.name = f"ruin_{key}"
    bisect(ruin, Vector((sx * 0.5, 0, sz * 0.45)), Vector((0.55, 0.15, 0.82)).normalized())
    bisect(ruin, Vector((-sx * 0.5, sy * 0.5, sz * 0.7)), Vector((-0.4, 0.45, 0.8)).normalized())
    bisect(ruin, Vector((0, -sy * 0.5, sz * 0.9)), Vector((0.1, -0.3, 0.95)).normalized())
    pieces[f"ruin_{key}"] = export(ruin, f"ruin_{key}")
    bpy.data.objects.remove(ruin)
    bpy.data.objects.remove(whole)

# surviving facade walls: a Lower Facade + Main Walls slab of the centre building cut to a thin jagged slice
objs = [o for o in objects_of(BUILDINGS["center"]) if re.sub(r"\.\d+$", "", o.name) in ("Main Walls", "Lower Facade", "Middle Window", "Lower Window", "Window Shutter", "Arched Window")]
if objs:
    for i, (depth, top) in enumerate(((0.12, 0.55), (0.16, 0.42))):
        chunk, size = join_copy(objs, f"wall_chunk_{'ab'[i]}")
        sx, sy, sz = size.x, size.y, size.z
        bisect(chunk, Vector((0, sy * 0.5 - sy * depth, 0)), Vector((0, -1, 0)))          # keep the street facade slab (+y side)
        bisect(chunk, Vector((sx * 0.3, 0, sz * top)), Vector((0.35, 0, 0.94)).normalized())
        bisect(chunk, Vector((-sx * 0.4, 0, sz * (top + 0.25))), Vector((-0.5, 0, 0.87)).normalized())
        pieces[f"wall_chunk_{'ab'[i]}"] = export(chunk, f"wall_chunk_{'ab'[i]}")
        bpy.data.objects.remove(chunk)

with open(os.path.join(OUT, "pieces.json"), "w", encoding="utf-8") as f:
    json.dump({"source": "3 Piece Parisian Apartment Building (user-supplied Blender pack)", "pieces": pieces}, f, indent=1)
log("DONE", list(pieces))
