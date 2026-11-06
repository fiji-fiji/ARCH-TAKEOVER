"""Build the level geometry and actor list from OSM data + data/map/layout.json.

Outputs (all under data/map/):
    meshes/*.obj            one OBJ per static mesh, in Unreal centimetres, x east / y north / z up
                            (UE's OBJ importer maps (x,y,z) -> (x,-y,z), i.e. +Y becomes south: no scaling)
    level.json              actors, player starts, bomb sites, blockers, callouts, nav bounds, materials
    ../preview_level.png    top-down check image

Usage: python -I tools/build_map.py
"""
import sys, json, math, pathlib, random, re
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib
from osm_lib import point_in_poly, poly_area, centroid, bbox_of, dist_point_seg
from pnglib import Canvas

ROOT = osm_lib.ROOT
MAP = ROOT / "data" / "map"
MESH_DIR = MAP / "meshes"
MESH_DIR.mkdir(parents=True, exist_ok=True)
_written = set()      # OBJ files produced by this run; unchanged ones keep their mtime (no Unreal re-import)


def _write_retry(path, text, tries=6):
    """Dropbox briefly locks files it is syncing: retry instead of crashing."""
    import time
    for k in range(tries):
        try:
            path.write_text(text, encoding="ascii"); return
        except OSError as e:
            if k == tries - 1:
                raise
            time.sleep(0.3 * (k + 1))

layout = json.loads((MAP / "layout.json").read_text(encoding="utf-8"))
osm = osm_lib.load()
rng = random.Random(42)

ZONE = [tuple(p) for p in layout["zone"]]
ZX0, ZY0, ZX1, ZY1 = bbox_of(ZONE)
MARGIN = layout["backdrop_margin"]
BX0, BY0, BX1, BY1 = ZX0 - MARGIN, ZY0 - MARGIN, ZX1 + MARGIN, ZY1 + MARGIN

KERB_H = 0.12          # sidewalk height (m)
SHOP_H = 4.0           # ground-floor band height (m)
FLOOR_H = 3.0


# ======================================================================= geometry helpers
def v_sub(a, b): return (a[0] - b[0], a[1] - b[1])
def v_len(a): return math.hypot(a[0], a[1])
def v_norm(a):
    L = v_len(a); return (a[0] / L, a[1] / L) if L > 1e-9 else (0.0, 0.0)
def rot2(p, deg):
    r = math.radians(deg); c, s = math.cos(r), math.sin(r)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c)


clean_ring = osm_lib.clean_ring          # shared with build_facades.py (same ring = same wall indices)

# photo facades (assets/facades/index.json, written by build_facades.py): building id -> atlas + wall rects
FAC_INDEX = ROOT / "assets" / "facades" / "index.json"
FACADES = json.loads(FAC_INDEX.read_text(encoding="utf-8")) if FAC_INDEX.exists() else {}


def triangulate(pts):
    """Ear clipping for a simple CCW polygon. Returns list of index triples."""
    n = len(pts)
    if n < 3:
        return []
    idx = list(range(n))
    tris = []

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def in_tri(p, a, b, c):
        return cross(a, b, p) >= -1e-9 and cross(b, c, p) >= -1e-9 and cross(c, a, p) >= -1e-9

    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        found = False
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            if cross(a, b, c) <= 1e-9:
                continue                          # reflex vertex
            ok = True
            for j in idx:
                if j in (i0, i1, i2):
                    continue
                if in_tri(pts[j], a, b, c):
                    ok = False; break
            if ok:
                tris.append((i0, i1, i2)); idx.pop(k); found = True; break
        if not found:                             # degenerate: fall back to fan
            for k in range(1, len(idx) - 1):
                tris.append((idx[0], idx[k], idx[k + 1]))
            return tris
    if len(idx) == 3:
        tris.append((idx[0], idx[1], idx[2]))
    return tris


class MeshWriter:
    """Accumulates faces per material and writes an OBJ in centimetres."""

    def __init__(self, name, origin=(0.0, 0.0)):
        self.name = name; self.origin = origin
        self.v, self.vt, self.vn = [], [], []
        self.faces = {}          # material -> list of faces [(vi, ti, ni), ...]
        self.ntri = 0

    def _add_face(self, mat, pts3, uvs):
        # pts3 are (x, y, z) metres in world; CCW seen from outside
        p0, p1, p2 = pts3[0], pts3[1], pts3[2]
        ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        wx, wy, wz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
        nx, ny, nz = uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx
        L = math.sqrt(nx * nx + ny * ny + nz * nz)
        if L < 1e-9:
            return
        self.vn.append((nx / L, ny / L, nz / L)); ni = len(self.vn)
        face = []
        for p, uv in zip(pts3, uvs):
            self.v.append((p[0] - self.origin[0], p[1] - self.origin[1], p[2])); self.vt.append(uv)
            face.append((len(self.v), len(self.vt), ni))
        self.faces.setdefault(mat, []).append(face)
        self.ntri += len(pts3) - 2

    def quad(self, mat, pts3, uvs):
        self._add_face(mat, pts3, uvs)

    def polygon_cap(self, mat, ring, z, uv_scale, up=True):
        """Horizontal polygon (CCW ring) at height z; world-projected UVs."""
        tris = triangulate(ring)
        for (a, b, c) in tris:
            ids = (a, b, c) if up else (a, c, b)
            pts3 = [(ring[i][0], ring[i][1], z) for i in ids]
            uvs = [(ring[i][0] / uv_scale, ring[i][1] / uv_scale) for i in ids]
            self._add_face(mat, pts3, uvs)

    def ortho_cap(self, ring, z, up=True):
        """Horizontal polygon textured with the orthophoto (falls back to asphalt when there is none)."""
        if not ORTHO:
            return self.polygon_cap("M_Asphalt", ring, z, 4.0, up=up)
        tris = triangulate(ring)
        for (a, b, c) in tris:
            ids = (a, b, c) if up else (a, c, b)
            self._add_face("M_Ground", [(ring[i][0], ring[i][1], z) for i in ids], [ortho_uv(ring[i]) for i in ids])

    def wall(self, mat, p, q, z0, z1, u0, u_scale, v_scale, v0=0.0):
        """Vertical quad from p to q (outward = right-hand side of p->q for a CCW ring)."""
        L = v_len(v_sub(q, p))
        u1 = u0 + L / u_scale
        self._add_face(mat,
                       [(p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1)],
                       [(u0, (z0 - v0) / v_scale), (u1, (z0 - v0) / v_scale), (u1, (z1 - v0) / v_scale), (u0, (z1 - v0) / v_scale)])

    def prism(self, ring, z0, z1, mat_side, mat_top, uv_side=1.0, uv_top=1.0, bottom=False, mat_bottom=None, eps=0.05):
        ring = clean_ring(ring, eps)
        if len(ring) < 3:
            return
        u = 0.0
        for i in range(len(ring)):
            p, q = ring[i], ring[(i + 1) % len(ring)]
            self.wall(mat_side, p, q, z0, z1, u, uv_side, uv_side)
            u += v_len(v_sub(q, p)) / uv_side
        self.polygon_cap(mat_top, ring, z1, uv_top, up=True)
        if bottom:
            self.polygon_cap(mat_bottom or mat_side, ring, z0, uv_top, up=False)

    def box(self, mat, center, size, z0, yaw=0.0, uv=1.0, mat_top=None):
        """Axis box (size = (sx, sy, sz)) rotated by yaw about its centre, resting on z0."""
        sx, sy, sz = size
        corners = [(-sx / 2, -sy / 2), (sx / 2, -sy / 2), (sx / 2, sy / 2), (-sx / 2, sy / 2)]
        ring = [(center[0] + rot2(c, yaw)[0], center[1] + rot2(c, yaw)[1]) for c in corners]
        self.prism(ring, z0, z0 + sz, mat, mat_top or mat, uv, uv, bottom=True, eps=min(sx, sy) * 0.25)   # thin parts survive

    def cylinder(self, mat, center, r, z0, z1, segs=12, mat_top=None, uv=1.0):
        ring = [(center[0] + r * math.cos(2 * math.pi * k / segs), center[1] + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
        self.prism(ring, z0, z1, mat, mat_top or mat, uv, uv, bottom=True)

    def sphere(self, mat, center, r, zc, rings=5, segs=10):
        """Low-poly sphere (lathe)."""
        for i in range(rings):
            a0 = -math.pi / 2 + math.pi * i / rings; a1 = -math.pi / 2 + math.pi * (i + 1) / rings
            r0, z0 = r * math.cos(a0), zc + r * math.sin(a0); r1, z1 = r * math.cos(a1), zc + r * math.sin(a1)
            for k in range(segs):
                t0 = 2 * math.pi * k / segs; t1 = 2 * math.pi * (k + 1) / segs
                p00 = (center[0] + r0 * math.cos(t0), center[1] + r0 * math.sin(t0), z0)
                p10 = (center[0] + r0 * math.cos(t1), center[1] + r0 * math.sin(t1), z0)
                p11 = (center[0] + r1 * math.cos(t1), center[1] + r1 * math.sin(t1), z1)
                p01 = (center[0] + r1 * math.cos(t0), center[1] + r1 * math.sin(t0), z1)
                uv = [(k / segs, i / rings), ((k + 1) / segs, i / rings), ((k + 1) / segs, (i + 1) / rings), (k / segs, (i + 1) / rings)]
                if r0 < 1e-6:
                    self._add_face(mat, [p00, p11, p01], [uv[0], uv[2], uv[3]])
                elif r1 < 1e-6:
                    self._add_face(mat, [p00, p10, p11], uv[:3])
                else:
                    self._add_face(mat, [p00, p10, p11, p01], uv)

    def cone(self, mat, center, r, z0, z1, segs=12):
        for k in range(segs):
            t0 = 2 * math.pi * k / segs; t1 = 2 * math.pi * (k + 1) / segs
            p0 = (center[0] + r * math.cos(t0), center[1] + r * math.sin(t0), z0)
            p1 = (center[0] + r * math.cos(t1), center[1] + r * math.sin(t1), z0)
            self._add_face(mat, [p0, p1, (center[0], center[1], z1)], [(0, 0), (1, 0), (0.5, 1)])

    def write(self):
        if not self.faces:
            return None
        path = MESH_DIR / f"{self.name}.obj"
        lines = [f"# {self.name}  (cm, x east, y north, z up)", f"mtllib {self.name}.mtl", f"o {self.name}"]
        for x, y, z in self.v:
            lines.append(f"v {x*100:.1f} {y*100:.1f} {z*100:.1f}")
        for u, v in self.vt:
            lines.append(f"vt {u:.4f} {v:.4f}")
        for x, y, z in self.vn:
            lines.append(f"vn {x:.4f} {y:.4f} {z:.4f}")
        slots = []
        for mat, faces in self.faces.items():
            slots.append(mat)
            lines.append(f"usemtl {mat}")
            for f in faces:
                lines.append("f " + " ".join(f"{vi}/{ti}/{ni}" for vi, ti, ni in f))
        text = "\n".join(lines) + "\n"
        _written.add(path.name)
        if not (path.exists() and path.read_text(encoding="ascii") == text):     # unchanged => keep the mtime
            _write_retry(path, text)
        mtl = MESH_DIR / f"{self.name}.mtl"; mtl_text = "".join(f"newmtl {m}\nKd 0.8 0.8 0.8\n" for m in slots)
        if not (mtl.exists() and mtl.read_text(encoding="ascii") == mtl_text):
            _write_retry(mtl, mtl_text)
        return {"file": f"meshes/{self.name}.obj", "slots": slots, "tris": self.ntri}


# ======================================================================= materials
MATERIALS = {
    "M_Facade":    {"texture": "facade_haussmann.png", "roughness": 0.9},
    "M_Shopfront": {"texture": "shopfront.png", "roughness": 0.5},
    "M_Roof":      {"texture": "roof_zinc.png", "roughness": 0.45, "metallic": 0.35},
    "M_Asphalt":   {"texture": "asphalt.png", "roughness": 0.95},
    "M_Road":      {"texture": "road.png", "roughness": 0.95},
    "M_Pavement":  {"texture": "pavement.png", "roughness": 0.9},
    "M_Kerb":      {"color": [0.50, 0.50, 0.49], "roughness": 0.8},
    "M_Grass":     {"texture": "grass.png", "roughness": 1.0},
    "M_Tarp":      {"texture": "tarp.png", "roughness": 0.8},
    "M_Bark":      {"color": [0.30, 0.22, 0.15], "roughness": 0.95},
    "M_Leaves":    {"color": [0.22, 0.42, 0.16], "roughness": 0.9},
    "M_Concrete":  {"color": [0.62, 0.60, 0.56], "roughness": 0.9},
    "M_Soil":      {"color": [0.25, 0.18, 0.12], "roughness": 1.0},
    "M_Metal":     {"color": [0.35, 0.36, 0.38], "roughness": 0.45, "metallic": 0.8},
    "M_Glass":     {"color": [0.10, 0.14, 0.18], "roughness": 0.1, "metallic": 0.6},
    "M_CarRed":    {"color": [0.65, 0.10, 0.08], "roughness": 0.3, "metallic": 0.5},
    "M_CarWhite":  {"color": [0.85, 0.85, 0.83], "roughness": 0.3, "metallic": 0.4},
    "M_CarBlue":   {"color": [0.12, 0.22, 0.50], "roughness": 0.3, "metallic": 0.5},
    "M_CarGrey":   {"color": [0.30, 0.31, 0.33], "roughness": 0.3, "metallic": 0.5},
    "M_Tyre":      {"color": [0.05, 0.05, 0.05], "roughness": 0.9},
    "M_BusGreen":  {"color": [0.05, 0.40, 0.25], "roughness": 0.35, "metallic": 0.3},
    "M_BusWhite":  {"color": [0.88, 0.88, 0.86], "roughness": 0.35, "metallic": 0.3},
    "M_KioskGreen":{"color": [0.10, 0.28, 0.20], "roughness": 0.5},
    "M_Wood":      {"color": [0.45, 0.30, 0.16], "roughness": 0.8},
    "M_Dumpster":  {"color": [0.12, 0.35, 0.18], "roughness": 0.6, "metallic": 0.3},
    "M_Container": {"color": [0.15, 0.30, 0.55], "roughness": 0.6, "metallic": 0.4},
    "M_Parasol":   {"color": [0.75, 0.12, 0.10], "roughness": 0.8},
    "M_BarrierGrey": {"color": [0.55, 0.56, 0.58], "roughness": 0.5, "metallic": 0.7},
    "M_Stone":     {"color": [0.78, 0.74, 0.66], "roughness": 0.85},
    "M_GunBlack":  {"color": [0.06, 0.06, 0.065], "roughness": 0.55, "metallic": 0.6},
    "M_Gear":      {"color": [0.16, 0.17, 0.14], "roughness": 0.9},
    "M_Skin":      {"color": [0.72, 0.55, 0.42], "roughness": 0.7},
    "M_Boot":      {"color": [0.08, 0.07, 0.06], "roughness": 0.8},
}
ORTHO_JSON = ROOT / "assets" / "ortho" / "ground.json"
ORTHO = json.loads(ORTHO_JSON.read_text(encoding="utf-8")) if ORTHO_JSON.exists() else None
if ORTHO:
    # the real ground: IGN orthophoto over the backdrop box, multiplied by the tiling asphalt detail
    MATERIALS["M_Ground"] = {"texture_path": "assets/ortho/ground.jpg", "detail": "asphalt.png",
                             "detail_tiling": [(ORTHO["bbox"][2] - ORTHO["bbox"][0]) / 4.0, (ORTHO["bbox"][3] - ORTHO["bbox"][1]) / 4.0],
                             "roughness": 0.9}


def ortho_uv(p):
    """World-projected UV into the orthophoto (OBJ v = 0 at the south edge; Unreal flips v on import)."""
    b = ORTHO["bbox"]
    return ((p[0] - b[0]) / (b[2] - b[0]), (p[1] - b[1]) / (b[3] - b[1]))

meshes = {}     # id -> {"file","slots","tris"}
actors = []     # placed instances


def emit(mw, loc=(0.0, 0.0, 0.0), yaw=0.0, tags=None, name=None):
    info = mw.write()
    if info is None:
        return
    meshes[mw.name] = info
    actors.append({"name": name or mw.name, "mesh": mw.name, "loc": ue_loc(loc), "yaw": ue_yaw(yaw), "tags": tags or []})


def ue_loc(p):
    return [round(p[0] * 100, 1), round(-p[1] * 100, 1), round((p[2] if len(p) > 2 else 0.0) * 100, 1)]


def ue_yaw(deg):
    return round(-deg, 2)


def in_box(p, x0, y0, x1, y1):
    return x0 <= p[0] <= x1 and y0 <= p[1] <= y1


def ring_touches(ring, x0, y0, x1, y1):
    bx0, by0, bx1, by1 = bbox_of(ring)
    return not (bx1 < x0 or bx0 > x1 or by1 < y0 or by0 > y1)


# ======================================================================= roads (needed for sidewalk widths)
road_segs = []   # (a, b, half_width, name, cls)
for r in osm["roads"]:
    cls = r["tags"]["highway"]
    if cls in ("footway", "path", "steps", "cycleway", "corridor", "elevator", "platform"):
        continue
    pts = r["pts"]
    if not ring_touches(pts, BX0, BY0, BX1, BY1):
        continue
    for a, b in zip(pts, pts[1:]):
        road_segs.append((a, b, r["width"] / 2.0, r["name"], cls))


def nearest_road(p):
    best = (1e9, None)
    for a, b, hw, name, cls in road_segs:
        d = dist_point_seg(p, a, b)
        if d < best[0]:
            best = (d, hw)
    return best


# ======================================================================= covered passages (corridors through buildings)
PASSAGES = []
for ps in layout.get("passages", []):
    a, b = tuple(ps["pts"][0]), tuple(ps["pts"][-1])
    d = v_norm(v_sub(b, a))
    PASSAGES.append({"a": a, "b": b, "d": d, "n": (-d[1], d[0]), "hw": ps["width"] / 2.0, "h": ps["height"], "L": v_len(v_sub(b, a)), "name": ps["name"]})


def in_passage(p):
    return any(dist_point_seg(p, ps["a"], ps["b"]) <= ps["hw"] for ps in PASSAGES)


def passage_hits(ring):
    out = []
    for ps in PASSAGES:
        a, d, L = ps["a"], ps["d"], ps["L"]
        if any(point_in_poly((a[0] + d[0] * s, a[1] + d[1] * s), ring) for s in range(0, int(L) + 1)):
            out.append(ps)
    return out


def clip_halfplane(ring, a, n, off):
    """Sutherland-Hodgman: keep the part of ring where dot(p - a, n) >= off."""
    out = []
    def val(p): return (p[0] - a[0]) * n[0] + (p[1] - a[1]) * n[1] - off
    for i in range(len(ring)):
        p, q = ring[i], ring[(i + 1) % len(ring)]
        vp, vq = val(p), val(q)
        if vp >= 0:
            out.append(p)
        if (vp >= 0) != (vq >= 0):
            t = vp / (vp - vq); out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return clean_ring(out)


# ======================================================================= buildings
building_rings = []        # cleaned rings for collision tests
n_detail = n_backdrop = 0
n_passages = 0
n_photo_bld = 0
n_photo_walls_counter = [0]
for b in osm["buildings"]:
    ring = clean_ring(b["pts"])
    if len(ring) < 3 or abs(poly_area(ring)) < 4:
        continue
    if not ring_touches(ring, BX0, BY0, BX1, BY1):
        continue
    detail = ring_touches(ring, ZX0 - 15, ZY0 - 15, ZX1 + 15, ZY1 + 15)
    h = b["height"]
    c = centroid(ring)
    mw = MeshWriter(f"bld_{b['id']}", origin=c)
    building_rings.append(ring)
    n = len(ring)
    u = 0.0
    hits = passage_hits(ring) if detail else []
    # photo-textured walls: atlas rect per wall index (same clean_ring => same indices as build_facades)
    fac = FACADES.get(b["id"]) if detail else None
    photo = {}
    if fac:
        mat = f"M_Fac_{b['id']}"
        MATERIALS[mat] = {"texture_path": fac["atlas"], "roughness": 0.85}
        n_photo_bld += 1
        for k, e in fac["faces"].items():
            photo[int(k)] = e
        aw, ah = fac["w"], fac["h"]

    def photo_wall(i, p, q, z0, z1):
        """Wall quad with UVs into the building's atlas; rows map z in [0, h] (OBJ v = 0 at the bottom)."""
        e = photo[i]
        u0, u1 = e["x0"] / aw, e["x1"] / aw
        def v_at(z):
            row = e["y1"] - (e["y1"] - e["y0"]) * (z / h)
            return 1.0 - row / ah
        mw.quad(mat, [(p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1)],
                [(u0, v_at(z0)), (u1, v_at(z0)), (u1, v_at(z1)), (u0, v_at(z1))])
        n_photo_walls_counter[0] += 1

    if hits:
        # covered passage: the ground floor is split either side of the corridor, upper floors bridge over it
        ps = hits[0]; ph = ps["h"]; n_passages += 1
        for side in (1.0, -1.0):
            nn = (ps["n"][0] * side, ps["n"][1] * side)
            piece = clip_halfplane(ring, ps["a"], nn, ps["hw"])
            if len(piece) >= 3:
                uu = 0.0
                for i in range(len(piece)):
                    p, q = piece[i], piece[(i + 1) % len(piece)]
                    mw.wall("M_Shopfront", p, q, 0.0, ph, uu / 4.0 * 3.0, 4.0, 4.0)
                    uu += v_len(v_sub(q, p))
        for i in range(n):
            p, q = ring[i], ring[(i + 1) % n]
            if i in photo:
                photo_wall(i, p, q, ph, h)
            else:
                mw.wall("M_Facade", p, q, ph, h, u / 3.0, 3.0, FLOOR_H, v0=ph)
            u += v_len(v_sub(q, p))
        mw.polygon_cap("M_Stone", ring, ph, 3.0, up=False)        # passage ceiling
        print(f"  passage '{ps['name']}' carved through {b['id']}")
    for i in range(n):
        if hits:
            break
        p, q = ring[i], ring[(i + 1) % n]
        L = v_len(v_sub(q, p))
        if i in photo:
            photo_wall(i, p, q, 0.0, h)                         # the real facade, shopfront included
        elif detail and h > SHOP_H + 1:
            mw.wall("M_Shopfront", p, q, 0.0, SHOP_H, u / 4.0 * 3.0, 4.0, 4.0)
            mw.wall("M_Facade", p, q, SHOP_H, h, u / 3.0, 3.0, FLOOR_H, v0=SHOP_H)
        else:
            mw.wall("M_Facade", p, q, 0.0, h, u / 3.0, 3.0, FLOOR_H, v0=h - FLOOR_H * math.floor(h / FLOOR_H))
        u += L
    # roof + mansard for tall buildings
    if h >= 12 and len(ring) <= 40:
        inset = []
        for p in ring:
            d = v_sub(p, c); inset.append((c[0] + d[0] * 0.80, c[1] + d[1] * 0.80))
        if abs(poly_area(inset)) > 1.0:
            top = h + 2.6
            for i in range(n):
                p, q = ring[i], ring[(i + 1) % n]; p2, q2 = inset[i], inset[(i + 1) % n]
                mw.quad("M_Roof", [(p[0], p[1], h), (q[0], q[1], h), (q2[0], q2[1], top), (p2[0], p2[1], top)],
                        [(0, 0), (v_len(v_sub(q, p)) / 3.0, 0), (v_len(v_sub(q, p)) / 3.0, 1), (0, 1)])
            mw.polygon_cap("M_Roof", inset, top, 3.0)
        else:
            mw.polygon_cap("M_Roof", ring, h, 3.0)
    else:
        mw.polygon_cap("M_Roof", ring, h, 3.0)
    # Haussmann balconies (continuous ledges) on detail buildings
    if detail and h >= 14:
        for zb in (SHOP_H + FLOOR_H * 1.0 + 1.3, SHOP_H + FLOOR_H * 4.0 + 1.3):
            if zb + 0.3 < h:
                for i in range(n):
                    if i in photo:
                        continue                                # the photo shows the real balconies
                    p, q = ring[i], ring[(i + 1) % n]
                    d = v_norm(v_sub(q, p)); nrm = (d[1], -d[0])
                    ledge = [p, q, (q[0] + nrm[0] * 0.45, q[1] + nrm[1] * 0.45), (p[0] + nrm[0] * 0.45, p[1] + nrm[1] * 0.45)]
                    if v_len(v_sub(q, p)) > 1.0:
                        mw.prism(ledge, zb, zb + 0.18, "M_Stone", "M_Stone", 1.0, 1.0, bottom=True)
    tags = ["building", "detail" if detail else "backdrop"]
    emit(mw, loc=(c[0], c[1], 0.0), tags=tags)
    if detail:
        n_detail += 1
    else:
        n_backdrop += 1


# ======================================================================= ground, roads, sidewalks, greens (chunked)
CHUNK = 100.0


def chunk_key(p):
    return (int(math.floor(p[0] / CHUNK)), int(math.floor(p[1] / CHUNK)))


ground = MeshWriter("ground")
ground.ortho_cap([(BX0, BY0), (BX1, BY0), (BX1, BY1), (BX0, BY1)], 0.0)
# invisible floor far below the city so a fall never goes forever (not needed, zone is fenced) – skipped
emit(ground, tags=["ground"])

road_chunks, side_chunks = {}, {}
for a, b, hw, name, cls in road_segs:
    d = v_norm(v_sub(b, a)); nrm = (-d[1], d[0]); L = v_len(v_sub(b, a))
    if L < 0.3:
        continue
    key = chunk_key(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2))
    mw = road_chunks.setdefault(key, MeshWriter(f"road_{key[0]}_{key[1]}".replace("-", "m")))
    z = 0.01
    quad = [(a[0] + nrm[0] * hw, a[1] + nrm[1] * hw, z), (a[0] - nrm[0] * hw, a[1] - nrm[1] * hw, z),
            (b[0] - nrm[0] * hw, b[1] - nrm[1] * hw, z), (b[0] + nrm[0] * hw, b[1] + nrm[1] * hw, z)]
    if ORTHO:
        mw.quad("M_Ground", quad, [ortho_uv(pt) for pt in quad])
    else:
        mw.quad("M_Road", quad, [(0, 1), (0, 0), (L / 4.0, 0), (L / 4.0, 1)])
    # joint discs at both ends (fill the wedge gaps at bends / junctions)
    for cpt in (a, b):
        disc = [(cpt[0] + hw * math.cos(2 * math.pi * k / 12), cpt[1] + hw * math.sin(2 * math.pi * k / 12)) for k in range(12)]
        mw.ortho_cap(disc, 0.005)
for key, mw in road_chunks.items():
    emit(mw, tags=["road"])

# sidewalks: ring around each building, width = gap to nearest carriageway edge (clamped)
for ring in building_rings:
    if not ring_touches(ring, BX0, BY0, BX1, BY1):
        continue
    n = len(ring)
    c = centroid(ring)
    key = chunk_key(c)
    mw = side_chunks.setdefault(key, MeshWriter(f"sidewalk_{key[0]}_{key[1]}".replace("-", "m")))
    offs = []
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        d = v_norm(v_sub(q, p)); nrm = (d[1], -d[0])
        mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
        dist, hw = nearest_road((mid[0] + nrm[0] * 2.0, mid[1] + nrm[1] * 2.0))
        w = 3.0 if hw is None else max(1.6, min(7.0, dist + 2.0 - hw))
        offs.append((nrm, w))
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        nrm, w = offs[i]
        if v_len(v_sub(q, p)) < 0.3:
            continue
        outer_p = (p[0] + nrm[0] * w, p[1] + nrm[1] * w); outer_q = (q[0] + nrm[0] * w, q[1] + nrm[1] * w)
        top = [p, q, outer_q, outer_p]
        if ORTHO:
            mw.ortho_cap(top, KERB_H)
        else:
            mw.polygon_cap("M_Pavement", top, KERB_H, 2.0)
        # kerb face on the outer edge (faces away from the building)
        mw.quad("M_Kerb", [(outer_q[0], outer_q[1], 0.0), (outer_p[0], outer_p[1], 0.0), (outer_p[0], outer_p[1], KERB_H), (outer_q[0], outer_q[1], KERB_H)],
                [(0, 0), (v_len(v_sub(q, p)), 0), (v_len(v_sub(q, p)), 0.12), (0, 0.12)])
        # convex corner wedge between this edge and the next
        nrm2, w2 = offs[(i + 1) % n]
        cross = nrm[0] * nrm2[1] - nrm[1] * nrm2[0]
        if cross < -1e-6:        # convex corner (outward normals turn clockwise)
            wedge = [q, (q[0] + nrm2[0] * w2, q[1] + nrm2[1] * w2), outer_q]
            if ORTHO:
                mw.ortho_cap(wedge, KERB_H)
            else:
                mw.polygon_cap("M_Pavement", wedge, KERB_H, 2.0)
            mw.quad("M_Kerb", [(wedge[1][0], wedge[1][1], 0.0), (outer_q[0], outer_q[1], 0.0), (outer_q[0], outer_q[1], KERB_H), (wedge[1][0], wedge[1][1], KERB_H)],
                    [(0, 0), (1, 0), (1, 0.12), (0, 0.12)])
for key, mw in side_chunks.items():
    emit(mw, tags=["sidewalk"])

# greens (park, lawns)
gi = 0
for g in osm["greens"]:
    ring = clean_ring(g["pts"])
    if len(ring) < 3 or not ring_touches(ring, BX0, BY0, BX1, BY1):
        continue
    mw = MeshWriter(f"green_{gi}"); gi += 1
    mw.prism(ring, 0.0, 0.10, "M_Kerb", "M_Grass", 1.0, 2.0)
    emit(mw, tags=["green"])


# ======================================================================= props (one mesh per type, many instances)
def make_tree(name, trunk_h, r):
    mw = MeshWriter(name)
    mw.cylinder("M_Bark", (0, 0), 0.22, 0.0, trunk_h, segs=8)
    mw.sphere("M_Leaves", (0, 0), r, trunk_h + r * 0.8, rings=5, segs=10)
    mw.sphere("M_Leaves", (0.4, 0.3), r * 0.7, trunk_h + r * 1.3, rings=4, segs=8)
    return mw


def make_car(name, body_mat):
    mw = MeshWriter(name)
    mw.box(body_mat, (0, 0), (4.3, 1.8, 0.65), 0.32)
    mw.box("M_Glass", (-0.3, 0), (2.2, 1.65, 0.55), 0.97)
    mw.box(body_mat, (-0.3, 0), (1.6, 1.5, 0.08), 1.52)
    for sx in (-1.4, 1.4):
        for sy in (-0.85, 0.85):
            ring = [(sx + 0.32 * math.cos(2 * math.pi * k / 10), sy + 0.12 * (1 if sy > 0 else -1)) for k in range(10)]
            # wheel: cylinder axis along y -> approximate with a short box
            mw.box("M_Tyre", (sx, sy), (0.64, 0.22, 0.64), 0.0)
    return mw


def make_van(name):
    mw = MeshWriter(name)
    mw.box("M_CarWhite", (0.3, 0), (4.0, 2.0, 1.9), 0.4)
    mw.box("M_CarWhite", (-2.2, 0), (1.1, 2.0, 0.9), 0.4)
    mw.box("M_Glass", (-1.75, 0), (0.3, 1.9, 0.9), 1.3)
    mw.box("M_CarWhite", (-1.6, 0), (0.6, 2.0, 0.9), 1.3)
    for sx in (-1.7, 1.5):
        for sy in (-0.95, 0.95):
            mw.box("M_Tyre", (sx, sy), (0.7, 0.24, 0.7), 0.0)
    return mw


def make_bus(name):
    mw = MeshWriter(name)
    mw.box("M_BusGreen", (0, 0), (12.0, 2.5, 1.1), 0.4)
    mw.box("M_BusWhite", (0, 0), (12.0, 2.5, 1.3), 1.5)
    mw.box("M_Glass", (0, 0), (11.6, 2.56, 1.0), 1.7)
    mw.box("M_BusWhite", (0, 0), (12.0, 2.5, 0.5), 2.8)
    for sx in (-4.0, 3.6):
        for sy in (-1.2, 1.2):
            mw.box("M_Tyre", (sx, sy), (1.0, 0.3, 1.0), 0.0)
    return mw


def make_planter(name):
    mw = MeshWriter(name)
    mw.box("M_Concrete", (0, 0), (2.0, 1.0, 0.9), 0.0, mat_top="M_Soil")
    mw.sphere("M_Leaves", (-0.5, 0), 0.55, 1.2, rings=4, segs=8)
    mw.sphere("M_Leaves", (0.5, 0.1), 0.5, 1.15, rings=4, segs=8)
    return mw


def make_kiosk(name):
    mw = MeshWriter(name)
    mw.box("M_KioskGreen", (0, 0), (3.0, 1.8, 2.6), 0.0)
    mw.box("M_KioskGreen", (0, 0), (3.6, 2.4, 0.2), 2.6)
    mw.box("M_Glass", (0, -0.93), (2.4, 0.08, 1.2), 1.0)
    mw.box("M_Stone", (0, 0), (3.4, 2.2, 0.08), 0.0)
    return mw


def make_column(name):
    mw = MeshWriter(name)
    mw.cylinder("M_KioskGreen", (0, 0), 0.95, 0.0, 0.25, segs=16)
    mw.cylinder("M_KioskGreen", (0, 0), 0.80, 0.25, 3.0, segs=16)
    mw.sphere("M_KioskGreen", (0, 0), 0.95, 3.0, rings=4, segs=16)
    mw.cylinder("M_KioskGreen", (0, 0), 0.25, 3.8, 4.4, segs=8)
    return mw


def make_bench(name):
    mw = MeshWriter(name)
    mw.box("M_Wood", (0, 0), (1.8, 0.5, 0.06), 0.45)
    mw.box("M_Wood", (0, 0.3), (1.8, 0.06, 0.45), 0.5)
    for sx in (-0.8, 0.8):
        mw.box("M_Metal", (sx, 0), (0.06, 0.5, 0.45), 0.0)
    return mw


def make_scaffold(name):
    mw = MeshWriter(name)
    for sx in (-3.0, 0.0, 3.0):
        for sy in (-0.6, 0.6):
            mw.box("M_Metal", (sx, sy), (0.06, 0.06, 6.0), 0.0)
    for z in (2.0, 4.0):
        mw.box("M_Wood", (0, 0), (6.0, 1.3, 0.06), z)
        mw.box("M_Metal", (0, -0.6), (6.0, 0.05, 0.05), z + 1.0)
        mw.box("M_Metal", (0, 0.6), (6.0, 0.05, 0.05), z + 1.0)
    mw.box("M_Tarp", (0, 0.68), (6.0, 0.03, 5.8), 0.1, uv=2.0)
    return mw


def make_dumpster(name):
    mw = MeshWriter(name)
    mw.box("M_Dumpster", (0, 0), (1.8, 1.0, 1.25), 0.15)
    mw.box("M_Dumpster", (0, 0), (1.85, 1.05, 0.12), 1.4)
    for sx in (-0.7, 0.7):
        for sy in (-0.4, 0.4):
            mw.box("M_Tyre", (sx, sy), (0.15, 0.15, 0.15), 0.0)
    return mw


def make_terrace(name):
    mw = MeshWriter(name)
    for k in range(3):
        x = -1.3 + k * 1.3
        mw.cylinder("M_Metal", (x, 0), 0.04, 0.0, 0.75, segs=6)
        mw.cylinder("M_Metal", (x, 0), 0.38, 0.72, 0.76, segs=10)
        mw.cylinder("M_Metal", (x, 0), 0.03, 0.76, 2.3, segs=6)
        mw.cone("M_Parasol", (x, 0), 1.05, 2.0, 2.45, segs=10)
        for sy in (-0.6, 0.6):
            mw.box("M_Wood", (x, sy), (0.42, 0.42, 0.45), 0.0)
    mw.box("M_Concrete", (0, 1.1), (4.0, 0.4, 0.8), 0.0, mat_top="M_Soil")
    mw.sphere("M_Leaves", (-1.2, 1.1), 0.35, 1.0, rings=3, segs=8)
    mw.sphere("M_Leaves", (0.0, 1.1), 0.35, 1.0, rings=3, segs=8)
    mw.sphere("M_Leaves", (1.2, 1.1), 0.35, 1.0, rings=3, segs=8)
    return mw


def make_barrier(name):
    mw = MeshWriter(name)
    for k in range(2):
        x = -1.3 + k * 2.6
        for sx in (-1.2, 1.2):
            mw.box("M_BarrierGrey", (x + sx, 0), (0.05, 0.4, 1.1), 0.0)
        mw.box("M_BarrierGrey", (x, 0), (2.5, 0.05, 0.08), 1.0)
        mw.box("M_BarrierGrey", (x, 0), (2.5, 0.05, 0.08), 0.35)
        for j in range(5):
            mw.box("M_BarrierGrey", (x - 1.0 + j * 0.5, 0), (0.03, 0.03, 0.6), 0.4)
    return mw


def make_container(name):
    mw = MeshWriter(name)
    mw.box("M_Container", (0, 0), (6.0, 2.4, 2.6), 0.0)
    for k in range(8):
        mw.box("M_Container", (-2.8 + k * 0.8, 0), (0.1, 2.5, 2.5), 0.05)
    return mw


def make_rifle(name):
    """Assault rifle along +x (forward), origin at the grip, metres."""
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.10, 0), (0.36, 0.05, 0.07), -0.035)            # receiver
    mw.box("M_GunBlack", (0.42, 0), (0.26, 0.045, 0.05), -0.025)           # handguard
    mw.box("M_Metal", (0.70, 0), (0.30, 0.022, 0.022), -0.011)             # barrel
    mw.box("M_GunBlack", (0.03, 0), (0.06, 0.035, 0.18), -0.20)            # magazine
    mw.box("M_GunBlack", (-0.09, 0), (0.05, 0.035, 0.12), -0.14)           # grip
    mw.box("M_GunBlack", (-0.32, 0), (0.28, 0.04, 0.06), -0.03)            # stock
    mw.box("M_GunBlack", (-0.44, 0), (0.04, 0.045, 0.10), -0.05)           # butt plate
    mw.box("M_GunBlack", (0.02, 0), (0.04, 0.03, 0.03), 0.035)             # rear sight
    mw.box("M_GunBlack", (0.80, 0), (0.02, 0.01, 0.035), 0.011)            # front sight post
    mw.box("M_Metal", (0.16, 0), (0.14, 0.03, 0.03), 0.035)                # top rail / optic
    return mw


def make_soldier(name):
    """Low-poly soldier, 1.8 m, facing +x, feet at z = 0, holding a rifle. Slot 0 (M_Uniform) is team-tinted."""
    mw = MeshWriter(name)
    for sy in (-0.11, 0.11):
        mw.box("M_Uniform", (0.0, sy), (0.20, 0.17, 0.80), 0.06)           # legs
        mw.box("M_Boot", (0.03, sy), (0.28, 0.18, 0.08), 0.0)               # boots
    mw.box("M_Uniform", (0.0, 0), (0.26, 0.44, 0.62), 0.86)                # torso
    mw.box("M_Gear", (0.0, 0), (0.32, 0.46, 0.40), 0.98)                   # plate carrier
    mw.box("M_Gear", (0.0, 0), (0.22, 0.36, 0.05), 1.48)                   # collar
    mw.sphere("M_Skin", (0.0, 0), 0.115, 1.60, rings=5, segs=10)           # head
    mw.sphere("M_Gear", (0.0, 0), 0.14, 1.645, rings=4, segs=10)           # helmet
    mw.box("M_Gear", (0.10, 0), (0.10, 0.26, 0.06), 1.60)                  # helmet brim
    for sy in (-0.21, 0.21):
        mw.box("M_Uniform", (0.10, sy), (0.34, 0.11, 0.11), 1.26)          # upper arms forward
        mw.box("M_Uniform", (0.32, sy * 0.55), (0.30, 0.10, 0.10), 1.24)   # forearms toward the rifle
        mw.box("M_Skin", (0.49, sy * 0.45), (0.09, 0.09, 0.09), 1.245)     # hands
    rifle = make_rifle("_rifle_tmp")                                        # the rifle in the hands
    for mat, faces in rifle.faces.items():
        for f in faces:
            pts = [rifle.v[vi - 1] for vi, ti, ni in f]; uvs = [rifle.vt[ti - 1] for vi, ti, ni in f]
            mw._add_face(mat, [(x + 0.30, y + 0.0, z + 1.27) for (x, y, z) in pts], uvs)
    return mw


MATERIALS["M_Uniform"] = {"color": [0.35, 0.36, 0.30], "roughness": 0.9}
for name, fn in (("soldier", make_soldier), ("rifle", make_rifle)):
    meshes[name] = fn(name).write()


# ======================================================================= weapon roster models (G11)
# Every gun: +x forward, origin at the top of the grip, metres; realistic silhouettes built from boxes.
def _sights(mw, rear_x, front_x, z=0.035):
    mw.box("M_GunBlack", (rear_x, 0), (0.03, 0.03, 0.025), z)
    mw.box("M_GunBlack", (front_x, 0), (0.015, 0.01, 0.03), z - 0.01)


def make_pistol(name, suppressor=False, long_mag=False):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.06, 0), (0.21, 0.028, 0.034), 0.0)                 # slide
    mw.box("M_GunBlack", (0.05, 0), (0.17, 0.026, 0.03), -0.03)                # frame
    mw.box("M_Metal", (0.17, 0), (0.03, 0.012, 0.012), 0.008)                  # muzzle
    mw.box("M_GunBlack", (-0.03, 0), (0.036, 0.03, 0.12), -0.15)               # grip
    mw.box("M_GunBlack", (0.015, 0), (0.045, 0.01, 0.012), -0.048)             # trigger guard
    mw.box("M_Metal", (0.0, 0), (0.008, 0.006, 0.02), -0.04)                   # trigger
    if long_mag:
        mw.box("M_GunBlack", (-0.03, 0), (0.028, 0.024, 0.12), -0.27)          # extended magazine
    if suppressor:
        mw.box("M_Metal", (0.26, 0), (0.15, 0.03, 0.03), -0.005)               # suppressor can
    _sights(mw, -0.02, 0.15, 0.034)
    return mw


def make_machine_pistol(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.07, 0), (0.26, 0.042, 0.06), -0.03)                # boxy receiver
    mw.box("M_GunBlack", (-0.03, 0), (0.036, 0.032, 0.11), -0.14)              # grip
    mw.box("M_GunBlack", (-0.03, 0), (0.03, 0.026, 0.16), -0.30)               # long magazine
    mw.box("M_Metal", (0.23, 0), (0.07, 0.018, 0.018), 0.0)                    # barrel
    mw.box("M_Metal", (0.27, 0), (0.025, 0.026, 0.026), -0.004)                # threaded tip
    mw.box("M_Metal", (-0.22, 0), (0.18, 0.012, 0.012), 0.0)                   # wire stock
    mw.box("M_Metal", (-0.30, 0), (0.012, 0.04, 0.06), -0.04)                  # stock plate
    mw.box("M_GunBlack", (0.10, 0), (0.05, 0.01, 0.012), -0.05)                # trigger guard
    _sights(mw, -0.04, 0.18, 0.03)
    return mw


def make_revolver(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.02, 0), (0.13, 0.03, 0.04), -0.02)                 # frame
    mw.box("M_Metal", (0.02, 0), (0.045, 0.04, 0.045), -0.022)                 # cylinder
    mw.box("M_Metal", (0.16, 0), (0.17, 0.02, 0.026), 0.0)                     # barrel
    mw.box("M_Metal", (0.16, 0), (0.17, 0.014, 0.012), -0.012)                 # under-lug
    mw.box("M_Wood", (-0.035, 0), (0.035, 0.03, 0.11), -0.13)                  # wooden grip
    mw.box("M_GunBlack", (0.0, 0), (0.045, 0.01, 0.012), -0.045)               # trigger guard
    mw.box("M_Metal", (-0.05, 0), (0.012, 0.01, 0.03), 0.01)                   # hammer
    _sights(mw, -0.02, 0.23, 0.026)
    return mw


def make_sawn_off(name):
    mw = MeshWriter(name)
    for sy in (-0.013, 0.013):
        mw.box("M_Metal", (0.20, sy), (0.30, 0.022, 0.022), 0.0)               # twin barrels
    mw.box("M_GunBlack", (0.0, 0), (0.13, 0.05, 0.055), -0.03)                 # receiver
    mw.box("M_Wood", (-0.04, 0), (0.04, 0.034, 0.12), -0.15)                   # pistol grip
    mw.box("M_Wood", (0.17, 0), (0.12, 0.05, 0.03), -0.04)                     # fore-end
    mw.box("M_GunBlack", (-0.01, 0), (0.045, 0.01, 0.012), -0.05)              # trigger guard
    mw.box("M_Metal", (0.34, 0), (0.012, 0.01, 0.02), 0.022)                   # bead
    return mw


def make_smg(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.10, 0), (0.32, 0.045, 0.06), -0.03)                # receiver
    mw.box("M_GunBlack", (0.24, 0), (0.12, 0.05, 0.055), -0.035)               # front shroud
    mw.box("M_GunBlack", (0.09, 0), (0.035, 0.03, 0.18), -0.21)                # magazine (in front of the grip)
    mw.box("M_GunBlack", (-0.04, 0), (0.04, 0.034, 0.12), -0.15)               # grip
    mw.box("M_Metal", (0.33, 0), (0.08, 0.02, 0.02), -0.004)                   # barrel
    mw.box("M_Metal", (-0.26, 0), (0.22, 0.016, 0.016), 0.0)                   # folding stock bars
    mw.box("M_Metal", (-0.37, 0), (0.014, 0.04, 0.08), -0.05)                  # butt
    mw.box("M_GunBlack", (0.02, 0), (0.05, 0.01, 0.012), -0.05)                # trigger guard
    mw.box("M_Metal", (0.10, 0), (0.16, 0.03, 0.02), 0.03)                     # top rail
    _sights(mw, -0.05, 0.27, 0.03)
    return mw


def make_pump_shotgun(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.05, 0), (0.24, 0.045, 0.06), -0.03)                # receiver
    mw.box("M_Metal", (0.45, 0), (0.52, 0.022, 0.022), 0.012)                  # barrel
    mw.box("M_Metal", (0.40, 0), (0.44, 0.02, 0.02), -0.016)                   # tube magazine
    mw.box("M_Wood", (0.33, 0), (0.14, 0.042, 0.042), -0.03)                   # pump
    mw.box("M_Wood", (-0.24, 0), (0.30, 0.04, 0.06), -0.04)                    # stock
    mw.box("M_Wood", (-0.07, 0), (0.05, 0.036, 0.09), -0.11)                   # grip part of the stock
    mw.box("M_Boot", (-0.40, 0), (0.025, 0.045, 0.09), -0.06)                  # rubber butt pad
    mw.box("M_GunBlack", (0.0, 0), (0.05, 0.01, 0.012), -0.05)                 # trigger guard
    mw.box("M_Metal", (0.70, 0), (0.012, 0.01, 0.02), 0.03)                    # bead
    return mw


def make_scout(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.08, 0), (0.30, 0.04, 0.05), -0.025)                # receiver
    mw.box("M_Metal", (0.56, 0), (0.58, 0.018, 0.018), 0.0)                    # barrel
    mw.box("M_GunBlack", (0.08, 0), (0.20, 0.03, 0.03), 0.055)                 # scope tube
    mw.box("M_Glass", (0.19, 0), (0.01, 0.034, 0.034), 0.053)                  # objective lens
    for x in (0.02, 0.14):
        mw.box("M_GunBlack", (x, 0), (0.02, 0.02, 0.035), 0.025)               # scope rings
    mw.box("M_GunBlack", (0.05, 0), (0.07, 0.03, 0.07), -0.095)                # magazine
    mw.box("M_Wood", (-0.28, 0), (0.30, 0.04, 0.06), -0.05)                    # stock
    mw.box("M_Wood", (-0.08, 0), (0.06, 0.036, 0.10), -0.12)                   # grip
    mw.box("M_Wood", (0.30, 0), (0.22, 0.04, 0.035), -0.04)                    # fore-end
    mw.box("M_Metal", (-0.02, 0.04), (0.03, 0.05, 0.015), 0.015)               # bolt handle
    mw.box("M_GunBlack", (-0.01, 0), (0.05, 0.01, 0.012), -0.05)               # trigger guard
    return mw


def make_long_rifle(name):
    mw = MeshWriter(name)
    mw.box("M_GunBlack", (0.10, 0), (0.38, 0.05, 0.07), -0.035)                # receiver / chassis
    mw.box("M_Metal", (0.72, 0), (0.52, 0.026, 0.026), -0.002)                 # heavy barrel
    mw.box("M_Metal", (0.99, 0), (0.06, 0.036, 0.036), -0.007)                 # muzzle brake
    mw.box("M_GunBlack", (0.12, 0), (0.28, 0.04, 0.04), 0.06)                  # big scope
    mw.box("M_Glass", (0.26, 0), (0.012, 0.046, 0.046), 0.057)                 # objective
    for x in (0.03, 0.19):
        mw.box("M_GunBlack", (x, 0), (0.025, 0.025, 0.04), 0.03)               # rings
    mw.box("M_GunBlack", (0.06, 0), (0.08, 0.035, 0.09), -0.125)               # magazine
    mw.box("M_GunBlack", (-0.06, 0), (0.045, 0.034, 0.12), -0.15)              # pistol grip
    mw.box("M_GunBlack", (-0.32, 0), (0.34, 0.045, 0.08), -0.06)               # skeleton stock
    mw.box("M_GunBlack", (-0.30, 0), (0.14, 0.045, 0.03), 0.02)                # cheek rest
    mw.box("M_Boot", (-0.50, 0), (0.025, 0.05, 0.11), -0.075)                  # butt pad
    for sy in (-0.03, 0.03):
        mw.box("M_Metal", (0.55, sy), (0.14, 0.01, 0.01), -0.035)              # folded bipod
    mw.box("M_Metal", (0.0, 0.045), (0.03, 0.05, 0.015), 0.01)                 # bolt handle
    mw.box("M_GunBlack", (-0.02, 0), (0.05, 0.01, 0.012), -0.055)              # trigger guard
    return mw


def make_knife(name):
    mw = MeshWriter(name)
    mw.box("M_Metal", (0.11, 0), (0.17, 0.004, 0.032), -0.016)                 # blade
    mw.box("M_Metal", (0.195, 0), (0.03, 0.004, 0.02), -0.01)                  # tip (narrower)
    mw.box("M_GunBlack", (0.02, 0), (0.015, 0.02, 0.05), -0.025)               # guard
    mw.box("M_Wood", (-0.05, 0), (0.12, 0.022, 0.034), -0.017)                 # handle
    mw.box("M_GunBlack", (-0.115, 0), (0.012, 0.024, 0.036), -0.018)           # pommel
    return mw


def make_grenade(name, kind):
    mw = MeshWriter(name)
    if kind == "frag":
        mw.cylinder("M_NadeGreen", (0, 0), 0.032, 0.0, 0.085, segs=10)         # body
        mw.cylinder("M_Metal", (0, 0), 0.014, 0.085, 0.11, segs=8)              # fuse head
        mw.box("M_Metal", (0.02, 0), (0.06, 0.008, 0.006), 0.10)                # lever
    elif kind == "smoke":
        mw.cylinder("M_Metal", (0, 0), 0.03, 0.0, 0.13, segs=10)                # can
        mw.cylinder("M_GunBlack", (0, 0), 0.031, 0.10, 0.13, segs=10)           # top band
        mw.box("M_Metal", (0.015, 0), (0.05, 0.008, 0.006), 0.125)              # lever
    else:
        mw.cylinder("M_Metal", (0, 0), 0.024, 0.0, 0.11, segs=10)               # flashbang tube
        for z in (0.02, 0.06):
            mw.cylinder("M_GunBlack", (0, 0), 0.025, z, z + 0.015, segs=10)     # stripes
        mw.box("M_Metal", (0.012, 0), (0.045, 0.008, 0.006), 0.105)             # lever
    return mw


def make_c4(name):
    mw = MeshWriter(name)
    mw.box("M_Gear", (0, 0), (0.26, 0.18, 0.07), 0.0)                           # charge
    mw.box("M_GunBlack", (0, 0), (0.14, 0.08, 0.02), 0.07)                      # detonator panel
    mw.box("M_Bomb", (0.05, 0.025), (0.015, 0.015, 0.01), 0.09)                 # red light
    return mw


MATERIALS["M_NadeGreen"] = {"color": [0.18, 0.24, 0.14], "roughness": 0.7}
MATERIALS["M_Bomb"] = {"color": [0.15, 0.12, 0.10], "roughness": 0.5, "metallic": 0.3, "emissive": [2.0, 0.05, 0.02]}
MATERIALS["M_Smoke"] = {"color": [0.72, 0.72, 0.70], "roughness": 1.0}
WEAPON_MESHES = {
    "gun_p9": lambda n: make_pistol(n), "gun_sp7": lambda n: make_pistol(n, suppressor=True),
    "gun_mp11": make_machine_pistol, "gun_r44": make_revolver, "gun_so12": make_sawn_off,
    "gun_smg9": make_smg, "gun_ps12": make_pump_shotgun, "gun_ar7": make_rifle,
    "gun_ar7s": lambda n: (lambda mw: (mw.box("M_Metal", (0.93, 0), (0.18, 0.035, 0.035), -0.017), mw)[1])(make_rifle(n)),
    "gun_sr30": make_scout, "gun_lr50": make_long_rifle, "gun_knife": make_knife,
    "nade_frag": lambda n: make_grenade(n, "frag"), "nade_smoke": lambda n: make_grenade(n, "smoke"),
    "nade_flash": lambda n: make_grenade(n, "flash"), "c4": make_c4,
}
for name, fn in WEAPON_MESHES.items():
    meshes[name] = fn(name).write()

PROP_BUILDERS = {
    "tree_a": lambda n: make_tree(n, 3.2, 2.6),
    "tree_b": lambda n: make_tree(n, 4.0, 3.4),
    "car_red": lambda n: make_car(n, "M_CarRed"), "car_white": lambda n: make_car(n, "M_CarWhite"),
    "car_blue": lambda n: make_car(n, "M_CarBlue"), "car_grey": lambda n: make_car(n, "M_CarGrey"),
    "van": make_van, "bus": make_bus, "planter": make_planter, "kiosk": make_kiosk, "column": make_column,
    "bench": make_bench, "scaffold": make_scaffold, "dumpster": make_dumpster, "terrace": make_terrace,
    "barrier": make_barrier, "container": make_container,
}
for name, fn in PROP_BUILDERS.items():
    info = fn(name).write()
    meshes[name] = info

PROP_FOOTPRINT = {   # (sx, sy) in metres, for collision tests / blocking props placement
    "car": (4.3, 1.8), "van": (5.1, 2.0), "bus": (12.0, 2.5), "planter": (2.0, 1.0), "kiosk": (3.6, 2.4),
    "column": (1.9, 1.9), "bench": (1.8, 0.5), "scaffold": (6.0, 1.4), "dumpster": (1.85, 1.05),
    "terrace": (4.0, 2.6), "barrier": (5.2, 0.4), "container": (6.0, 2.4),
}


def inside_any_building(p):
    if in_passage(p):
        return False
    for ring in building_rings:
        bx0, by0, bx1, by1 = bbox_of(ring)
        if bx0 <= p[0] <= bx1 and by0 <= p[1] <= by1 and point_in_poly(p, ring):
            return True
    return False


prop_rings = []
for i, pr in enumerate(layout["props"]["list"]):
    t = pr["type"]; at = tuple(pr["at"]); rot = pr.get("rot", 0.0)
    if inside_any_building(at):
        print(f"  WARN prop {t} at {at} is inside a building – skipped")
        continue
    mesh = t
    if t == "car":
        mesh = rng.choice(["car_red", "car_white", "car_blue", "car_grey"])
    actors.append({"name": f"prop_{t}_{i}", "mesh": mesh, "loc": ue_loc((at[0], at[1], KERB_H if t in ("planter", "kiosk", "column", "bench", "terrace") else 0.0)),
                   "yaw": ue_yaw(rot), "tags": ["prop", t]})
    sx, sy = PROP_FOOTPRINT[t]
    prop_rings.append([(at[0] + rot2(c, rot)[0], at[1] + rot2(c, rot)[1]) for c in [(-sx / 2, -sy / 2), (sx / 2, -sy / 2), (sx / 2, sy / 2), (-sx / 2, sy / 2)]])

# ======================================================================= war zone (layout.warzone): user-supplied building pieces + dressing
WAR = layout.get("warzone", {})
MATERIALS.update({
    "M_Sandbag":   {"color": [0.58, 0.52, 0.38], "roughness": 1.0},
    "M_Rust":      {"color": [0.30, 0.17, 0.10], "roughness": 0.85, "metallic": 0.3},
    "M_Burnt":     {"color": [0.07, 0.065, 0.06], "roughness": 0.95},
    "M_Scorch":    {"color": [0.05, 0.045, 0.04], "roughness": 1.0},
    "M_BrickRed":  {"color": [0.48, 0.26, 0.20], "roughness": 0.95},
    "M_SmokeDark": {"color": [0.22, 0.21, 0.20], "roughness": 1.0},
    "M_Fire":      {"color": [1.0, 0.45, 0.1], "roughness": 0.9, "emissive": [6.0, 1.6, 0.2]},
    "M_Flash":     {"color": [1.0, 0.7, 0.3], "roughness": 1.0, "emissive": [40.0, 18.0, 4.0]},
})


def make_rubble(name, seed, radius=2.2, count=16):
    """A mound of broken masonry: random boxes piled on a low cone."""
    r = random.Random(seed)
    mw = MeshWriter(name)
    mw.cone("M_Soil", (0, 0), radius, 0.0, radius * 0.45, segs=12)
    for i in range(count):
        a = r.uniform(0, 2 * math.pi); d = r.uniform(0, radius * 0.9)
        sx, sy, sz = r.uniform(0.3, 1.3), r.uniform(0.25, 0.9), r.uniform(0.15, 0.6)
        z0 = max(0.0, (1 - d / radius) * radius * 0.4 - sz * 0.5)
        mat = r.choice(["M_Concrete", "M_Stone", "M_BrickRed", "M_Concrete"])
        mw.box(mat, (math.cos(a) * d, math.sin(a) * d), (sx, sy, sz), z0, yaw=r.uniform(0, 180))
    for i in range(3):                                                    # a few planks / beams
        a = r.uniform(0, 2 * math.pi); d = r.uniform(0, radius * 0.6)
        mw.box(r.choice(["M_Wood", "M_Rust"]), (math.cos(a) * d, math.sin(a) * d), (r.uniform(1.5, 3.0), 0.12, 0.12), r.uniform(0.2, 0.8), yaw=r.uniform(0, 180))
    return mw


def make_sandbags(name, length=3.0, rows=3):
    mw = MeshWriter(name)
    r = random.Random(7)
    for row in range(rows):
        n = int(length / 0.62)
        for i in range(n):
            x = -length / 2 + 0.31 + i * 0.62 + (0.31 if row % 2 else 0.0)
            mw.box("M_Sandbag", (x, r.uniform(-0.03, 0.03)), (0.6, 0.34, 0.24), row * 0.23, yaw=r.uniform(-6, 6))
    return mw


def make_hedgehog(name):
    mw = MeshWriter(name)
    mw.box("M_Rust", (0, 0), (2.0, 0.14, 0.14), 0.75, yaw=0)
    mw.box("M_Rust", (0, 0), (2.0, 0.14, 0.14), 0.75, yaw=90)
    mw.box("M_Rust", (0, 0), (0.14, 0.14, 1.65), 0.0)
    return mw


def make_jersey(name):
    mw = MeshWriter(name)
    mw.box("M_Concrete", (0, 0), (3.0, 0.62, 0.25), 0.0)
    mw.box("M_Concrete", (0, 0), (3.0, 0.42, 0.35), 0.25)
    mw.box("M_Concrete", (0, 0), (3.0, 0.24, 0.25), 0.60)
    return mw


def make_burnt_car(name):
    mw = MeshWriter(name)
    mw.box("M_Burnt", (0, 0), (4.2, 1.75, 0.55), 0.28)                    # body, sagging
    mw.box("M_Burnt", (-0.2, 0), (2.2, 1.6, 0.55), 0.83)                  # cabin, no glass
    mw.box("M_Rust", (1.9, 0), (0.5, 1.5, 0.25), 0.35)                    # bonnet edge
    for sx in (-1.35, 1.35):
        for sy in (-0.85, 0.85):
            mw.box("M_Tyre", (sx, sy), (0.55, 0.2, 0.42), 0.0)            # flat tyres
    mw.box("M_Scorch", (0, 0), (5.5, 2.6, 0.02), 0.0)                      # scorched ground
    return mw


def make_crater(name, radius=2.6):
    mw = MeshWriter(name)
    disc = [(radius * math.cos(2 * math.pi * k / 14), radius * math.sin(2 * math.pi * k / 14)) for k in range(14)]
    mw.polygon_cap("M_Scorch", disc, 0.015, 2.0)
    r = random.Random(11)
    for k in range(12):
        a = 2 * math.pi * k / 12 + r.uniform(-0.2, 0.2); d = radius * r.uniform(0.75, 1.05)
        mw.box(r.choice(["M_Asphalt", "M_Concrete", "M_Soil"]), (math.cos(a) * d, math.sin(a) * d), (r.uniform(0.3, 0.8), r.uniform(0.3, 0.6), r.uniform(0.1, 0.3)), 0.0, yaw=r.uniform(0, 180))
    return mw


def make_smoke_column(name, height=42.0):
    """A static plume: stacked spheres drifting with the wind, a fire glow at the base."""
    mw = MeshWriter(name)
    mw.box("M_Fire", (0, 0), (2.2, 2.2, 0.8), 0.0)
    for i in range(9):
        t = i / 8
        z = 1.5 + t * height
        rad = 1.4 + t * 6.0
        mw.sphere("M_SmokeDark", (t * t * 14.0, t * 3.0), rad, z, rings=4, segs=8)
    return mw


WAR_BUILDERS = {
    "rubble_a": lambda n: make_rubble(n, 1), "rubble_b": lambda n: make_rubble(n, 2, 3.0, 22), "rubble_c": lambda n: make_rubble(n, 3, 1.6, 10),
    "sandbags": make_sandbags, "hedgehog": make_hedgehog, "jersey": make_jersey, "burnt_car": make_burnt_car,
    "crater": make_crater, "smoke_column": make_smoke_column,
}
WAR_FOOTPRINT = {"rubble": (4.4, 4.4), "sandbags": (3.2, 1.0), "hedgehog": (2.0, 2.0), "jersey": (3.0, 0.7),
                 "burnt_car": (4.4, 1.9), "crater": (0.1, 0.1), "smoke_column": (2.0, 2.0)}
for name, fn in WAR_BUILDERS.items():
    meshes[name] = fn(name).write()


def footprint_ring(at, rot, sx, sy):
    return [(at[0] + rot2(c, rot)[0], at[1] + rot2(c, rot)[1]) for c in [(-sx / 2, -sy / 2), (sx / 2, -sy / 2), (sx / 2, sy / 2), (-sx / 2, sy / 2)]]


_SITE_BOXES = [(tuple(layout["site" + k]["center"]), tuple(layout["site" + k]["half"])) for k in ("A", "B")]
_SPAWNS = [(tuple(layout[k]["center"]), layout[k]["radius"]) for k in ("spawnT", "spawnCT")]


def spot_ok(at, rot, sx, sy, keep_lane, allow_sites=False):
    """A footprint is fine when its corners, centre and mid-edges are outside every building, outside the bomb sites,
    clear of the spawns, and (keep_lane) leave at least 3 m of carriageway beside the nearest road axis."""
    ring = footprint_ring(at, rot, sx + 1.0, sy + 1.0)
    samples = ring + [at] + [((ring[i][0] + ring[(i + 1) % 4][0]) / 2, (ring[i][1] + ring[(i + 1) % 4][1]) / 2) for i in range(4)]
    if any(inside_any_building(c) for c in samples):
        return False
    for c, h in _SITE_BOXES:
        if not allow_sites and any(abs(q[0] - c[0]) <= h[0] + 1 and abs(q[1] - c[1]) <= h[1] + 1 for q in samples):
            return False
    for c, r in _SPAWNS:
        if any(v_len(v_sub(q, c)) < r + 4 for q in samples):
            return False
    if keep_lane:
        d, hw = nearest_road(at)
        if hw is not None and d < min(sx, sy) / 2 + 3.0:
            return False
    return True


def snap_spot(at, rot, sx, sy, keep_lane=True, max_r=16.0, step=2.0, allow_sites=False):
    """The nearest free spot to the designer's intent (spiral search), or None."""
    cands = [(0.0, 0.0)]
    n = int(max_r / step)
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            if (i, j) != (0, 0) and math.hypot(i * step, j * step) <= max_r:
                cands.append((i * step, j * step))
    cands.sort(key=lambda d: math.hypot(*d))
    for dx, dy in cands:
        q = (at[0] + dx, at[1] + dy)
        if spot_ok(q, rot, sx, sy, keep_lane, allow_sites):
            return q
    return None


# user-supplied model pieces (assets/models/<pack>/pieces.json, exported by tools/import_blender_assets.py)
MODEL_PIECES = {}
for pj in (ROOT / "assets" / "models").glob("*/pieces.json"):
    pack = pj.parent.name
    for key, info in json.loads(pj.read_text(encoding="utf-8"))["pieces"].items():
        MODEL_PIECES[f"{pack}/{key}"] = info
n_models = 0
for i, m in enumerate(WAR.get("models", [])):
    at = tuple(m["at"]); rot = m.get("rot", 0.0)
    info = MODEL_PIECES.get(m["model"])
    if info is None:
        print(f"  WARN model {m['model']} not exported (run tools/import_blender_assets.py) – skipped")
        continue
    sx, sy = info["size"][0], info["size"][1]
    spot = snap_spot(at, rot, sx, sy, keep_lane=not m.get("may_block", False))
    if spot is None:
        print(f"  WARN model {m['model']} near {at}: no free spot within 16 m – skipped")
        continue
    moved = v_len(v_sub(spot, at))
    print(f"  model {m['model']:28s} at ({spot[0]:6.1f},{spot[1]:7.1f}) rot {rot:4.0f}  {sx:.1f}x{sy:.1f} m" + (f"  (moved {moved:.1f} m from {at})" if moved > 0.1 else ""))
    actors.append({"name": f"model_{i}_{m['model'].split('/')[-1]}", "model": m["model"], "loc": ue_loc((spot[0], spot[1], m.get("z", 0.0))),
                   "yaw": ue_yaw(rot), "tags": ["model", "warzone"]})
    prop_rings.append(footprint_ring(spot, rot, sx, sy))
    n_models += 1
n_war = 0
for i, pr in enumerate(WAR.get("props", [])):
    t = pr["type"]; at = tuple(pr["at"]); rot = pr.get("rot", 0.0)
    sx, sy = WAR_FOOTPRINT[t]
    spot = at if t == "smoke_column" else snap_spot(at, rot, sx, sy, keep_lane=False, max_r=14.0, allow_sites=True)
    if spot is None:
        print(f"  WARN war prop {t} near {at}: no free spot – skipped")
        continue
    mesh = t
    if t == "rubble":
        mesh = rng.choice(["rubble_a", "rubble_b", "rubble_c"])
    actors.append({"name": f"war_{t}_{i}", "mesh": mesh, "loc": ue_loc((spot[0], spot[1], pr.get("z", 0.0))), "yaw": ue_yaw(rot), "tags": ["prop", "warzone", t]})
    if t not in ("crater",):
        prop_rings.append(footprint_ring(spot, rot, sx, sy))
    n_war += 1
print(f"war zone: {n_models} model pieces, {n_war} dressing props")

# trees from OSM
n_trees = 0
for t in osm["trees"]:
    if not in_box(t, BX0, BY0, BX1, BY1) or inside_any_building(t):
        continue
    actors.append({"name": f"tree_{n_trees}", "mesh": rng.choice(["tree_a", "tree_a", "tree_b"]),
                   "loc": ue_loc((t[0], t[1], KERB_H)), "yaw": ue_yaw(rng.uniform(0, 360)), "tags": ["tree"]})
    n_trees += 1


# ======================================================================= gameplay markers
def wall_blocker(a, b, height, thickness, name):
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    L = v_len(v_sub(b, a)); ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    return {"name": name, "loc": ue_loc((cx, cy, height / 2)), "extent": [round(L * 50, 1), round(thickness * 50, 1), round(height * 50, 1)], "yaw": ue_yaw(ang)}


blockers = []
cut_barriers = []


def barriers_along(a, b, tag):
    """Visible police barriers along a wall segment wherever the ground is free of buildings."""
    L = v_len(v_sub(b, a)); d = v_norm(v_sub(b, a)); ang = math.degrees(math.atan2(d[1], d[0]))
    s = 2.7
    while s < L - 1.0:
        p = (a[0] + d[0] * s, a[1] + d[1] * s)
        if (not inside_any_building(p) and not inside_any_building((p[0] + d[0] * 2.5, p[1] + d[1] * 2.5))
                and not inside_any_building((p[0] - d[0] * 2.5, p[1] - d[1] * 2.5))):
            actors.append({"name": f"cutbarrier_{len(cut_barriers)}", "mesh": "barrier", "loc": ue_loc((p[0], p[1], 0.0)),
                           "yaw": ue_yaw(ang), "tags": ["prop", "barrier", tag]})
            cut_barriers.append(p)
        s += 5.4


for i in range(len(ZONE)):
    a, b = ZONE[i], ZONE[(i + 1) % len(ZONE)]
    blockers.append(wall_blocker(a, b, 40.0, 1.0, f"zone_wall_{i}"))
    barriers_along(a, b, "perimeter")
for c in layout["cutoffs"]:
    segs = list(zip(c["pts"], c["pts"][1:])) if "pts" in c else [(c["a"], c["b"])]
    for j, (a, b) in enumerate(segs):
        a, b = tuple(a), tuple(b)
        blockers.append(wall_blocker(a, b, 12.0, 0.6, "cutoff_" + c["name"].replace(" ", "_") + f"_{j}"))
        barriers_along(a, b, "cutoff")


def free_spots(center, radius, facing_deg):
    """Grid of candidate spots around a spawn centre, free of buildings / props, nearest first."""
    cands = []
    step = 2.5
    n = int(radius / step)
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            p = (center[0] + i * step, center[1] + j * step)
            if v_len(v_sub(p, center)) <= radius:
                cands.append(p)
    cands.sort(key=lambda p: v_len(v_sub(p, center)))
    good = []
    for p in cands:
        if inside_any_building(p) or any(point_in_poly(p, r) for r in prop_rings):
            continue
        # keep 1.2 m clearance from building walls
        if any(dist_point_seg(p, ring[k], ring[(k + 1) % len(ring)]) < 1.2 for ring in building_rings
               if ring_touches(ring, p[0] - 2, p[1] - 2, p[0] + 2, p[1] + 2) for k in range(len(ring))):
            continue
        if all(v_len(v_sub(p, g)) >= 2.2 for g in good):
            good.append(p)
    return good


player_starts = []
for team, key in ((0, "spawnT"), (1, "spawnCT")):
    sp = layout[key]
    good = free_spots(tuple(sp["center"]), sp["radius"], sp["facing_deg"])
    if len(good) < 5:
        print("  WARN only", len(good), "free spawn spots near", sp["center"])
    for k in range(5):
        p = good[k % max(1, len(good))]
        player_starts.append({"team": team, "loc": ue_loc((p[0], p[1], KERB_H + 1.0)), "yaw": ue_yaw(sp["facing_deg"]), "label": sp["label"]})

bomb_sites = []
for sid in ("A", "B"):
    s = layout["site" + sid]
    if inside_any_building(tuple(s["center"])):
        print("  WARN bomb site", sid, "centre is inside a building")
    bomb_sites.append({"id": sid, "label": s["label"], "loc": ue_loc((s["center"][0], s["center"][1], 2.0)),
                       "extent": [s["half"][0] * 100, s["half"][1] * 100, 400]})

callouts = [{"text": c["name"], "loc": ue_loc((c["at"][0], c["at"][1], 3.2))} for c in layout["callouts"]]
home = layout["home"]
nav_bounds = {"loc": ue_loc(((ZX0 + ZX1) / 2, (ZY0 + ZY1) / 2, 5.0)), "extent": [(ZX1 - ZX0) * 50 + 500, (ZY1 - ZY0) * 50 + 500, 1500]}

# ======================================================================= minimap texture (U17, FPS Reborn colours, north up)
MM = 1024
mm_side = max(ZX1 - ZX0, ZY1 - ZY0) + 24.0
mm_cx, mm_cy = (ZX0 + ZX1) / 2, (ZY0 + ZY1) / 2
MX0, MY0, MX1, MY1 = mm_cx - mm_side / 2, mm_cy - mm_side / 2, mm_cx + mm_side / 2, mm_cy + mm_side / 2
mm = Canvas(MM, MM, (224, 207, 162))                                   # street #e0cfa2
mm_s = MM / mm_side
def MP(p): return ((p[0] - MX0) * mm_s, (MY1 - p[1]) * mm_s)
for g in osm["greens"]:
    ring = clean_ring(g["pts"])
    if len(ring) >= 3 and ring_touches(ring, MX0, MY0, MX1, MY1):
        mm.polygon([MP(p) for p in ring], (176, 143, 94))                  # parks like crates #b08f5e
for ring in building_rings:
    if ring_touches(ring, MX0, MY0, MX1, MY1):
        mm.polygon([MP(p) for p in ring], (93, 78, 60))                    # buildings #5d4e3c
for ring in prop_rings:
    mm.polygon([MP(p) for p in ring], (176, 143, 94))
for bs in bomb_sites:
    cx, cy = bs["loc"][0] / 100, -bs["loc"][1] / 100; hx, hy = bs["extent"][0] / 100, bs["extent"][1] / 100
    mm.polygon([MP((cx - hx, cy - hy)), MP((cx + hx, cy - hy)), MP((cx + hx, cy + hy)), MP((cx - hx, cy + hy))], (246, 231, 194))   # site floor #f6e7c2
for i in range(len(ZONE)):
    mm.line(MP(ZONE[i]), MP(ZONE[(i + 1) % len(ZONE)]), (255, 255, 255), 3)
mm.save(ROOT / "assets" / "textures" / "minimap.png")
_ini = ROOT / "unreal" / "ParisBombMap" / "Config" / "DefaultGame.ini"
_txt = _ini.read_text(encoding="utf-8") if _ini.exists() else ""
_txt = re.sub(r"\n?\[ParisMap\][^\[]*", "", _txt).rstrip()
_txt += "\n\n[ParisMap]\n" + "".join(f"Minimap{k}={v:.2f}\n" for k, v in (("X0", MX0), ("Y0", MY0), ("X1", MX1), ("Y1", MY1)))
_ini.write_text(_txt, encoding="utf-8")
minimap = {"x0": MX0, "y0": MY0, "x1": MX1, "y1": MY1, "px": MM,
           "sites": [{"id": bs["id"], "x": bs["loc"][0] / 100, "y": -bs["loc"][1] / 100} for bs in bomb_sites]}

level = {
    "name": layout["name"],
    "units": "Unreal cm; X east, Y south, Z up; yaw degrees (UE convention)",
    "origin": {"lat": osm_lib.ORIGIN_LAT, "lon": osm_lib.ORIGIN_LON, "address": "347 Rue Saint-Martin, 75003 Paris"},
    "textures_dir": "assets/textures",
    "materials": MATERIALS,
    "meshes": meshes,
    "actors": actors,
    "player_starts": player_starts,
    "bomb_sites": bomb_sites,
    "blockers": blockers,
    "callouts": callouts,
    "home": {"label": home["label"], "loc": ue_loc((home["center"][0], home["center"][1], 0.0))},
    "nav_bounds": nav_bounds,
    "zone_bbox_cm": [ZX0 * 100, -ZY1 * 100, ZX1 * 100, -ZY0 * 100],
    "sun": {"yaw": WAR.get("mood", {}).get("sun_yaw", 35), "pitch": WAR.get("mood", {}).get("sun_pitch", -48)},
    "minimap": minimap,
    "mood": WAR.get("mood"),
}
(MAP / "level.json").write_text(json.dumps(level, indent=1), encoding="utf-8")
for old in MESH_DIR.glob("*.obj"):                 # meshes that no longer exist
    if old.name not in _written:
        for victim in (old, old.with_suffix(".mtl")):
            try:
                victim.unlink(missing_ok=True)
            except OSError as e:
                print("  WARN could not delete", victim.name, e)
total_tris = sum(m["tris"] for m in meshes.values())
print(f"photo facades: {n_photo_bld} buildings, {n_photo_walls_counter[0]} walls (assets/facades)")
print(f"buildings: {n_detail} detail + {n_backdrop} backdrop; road chunks {len(road_chunks)}; sidewalk chunks {len(side_chunks)}; greens {gi}")
print(f"trees {n_trees}; props + war footprints {len(prop_rings)}; cut-off barriers {len(cut_barriers)}; blockers {len(blockers)}")
print(f"meshes {len(meshes)} ({total_tris} triangles); actors {len(actors)}; player starts {len(player_starts)}; sites {len(bomb_sites)}")

# ======================================================================= preview image
S = 2.0
W, H = int((BX1 - BX0) * S) + 1, int((BY1 - BY0) * S) + 1
c = Canvas(W, H, (40, 40, 42))
def P(p): return ((p[0] - BX0) * S, (BY1 - p[1]) * S)
for ring in building_rings:
    c.polygon([P(p) for p in ring], (120, 112, 100))
for a, b, hw, name, cls in road_segs:
    c.line(P(a), P(b), (70, 70, 72), hw * 2 * S)
for g in osm["greens"]:
    ring = clean_ring(g["pts"])
    if ring_touches(ring, BX0, BY0, BX1, BY1):
        c.polygon([P(p) for p in ring], (90, 150, 80))
for ring in prop_rings:
    c.polygon([P(p) for p in ring], (230, 200, 60))
for p in cut_barriers:
    x, y = P(p); c.circle(x, y, 3, (255, 120, 0))
zpts = [P(p) for p in ZONE]
for a, b in zip(zpts, zpts[1:] + zpts[:1]):
    c.line(a, b, (0, 120, 255), 3)
for cc in layout["cutoffs"]:
    segs = list(zip(cc["pts"], cc["pts"][1:])) if "pts" in cc else [(cc["a"], cc["b"])]
    for a, b in segs:
        c.line(P(a), P(b), (255, 60, 60), 3)
for ps in player_starts:
    x, y = P((ps["loc"][0] / 100, -ps["loc"][1] / 100)); c.circle(x, y, 4, (255, 200, 0) if ps["team"] == 0 else (0, 160, 255))
for bs in bomb_sites:
    cx, cy = bs["loc"][0] / 100, -bs["loc"][1] / 100; hx, hy = bs["extent"][0] / 100, bs["extent"][1] / 100
    r = [P((cx - hx, cy - hy)), P((cx + hx, cy - hy)), P((cx + hx, cy + hy)), P((cx - hx, cy + hy))]
    for a, b in zip(r, r[1:] + r[:1]):
        c.line(a, b, (255, 80, 0), 3)
    x, y = P((cx, cy)); c.text(int(x) - 5, int(y) - 7, bs["id"], (255, 255, 255), 3)
for co in layout["callouts"]:
    x, y = P(co["at"]); c.text(int(x) - len(co["name"]) * 3, int(y) + 6, co["name"], (255, 255, 255), 1)
x, y = P((0, 0)); c.circle(x, y, 5, (255, 0, 0))
c.save(ROOT / "data" / "preview_level.png")
print("wrote", MAP / "level.json", "and data/preview_level.png")
