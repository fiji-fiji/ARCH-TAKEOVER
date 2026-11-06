"""Project street-level photos onto the real building facades -> one texture atlas per building.

For every street-facing wall of every detailed building (same footprints + heights as build_map.py):
  1. collect the photos that can see it: camera on the outer side, not too grazing, nothing built
     between the camera and the wall (2-D line-of-sight against all footprints);
  2. for every texel of the wall (ppm pixels per metre), pick the candidate photo with the best
     pixel density on that spot (closer, more frontal, nearer the frame centre; the City of Paris
     campaign preferred) and sample it (bilinear), using a pinhole model for the 95° frames and the
     equirectangular model for the panoramas;
  3. pack the walls of the building into one JPEG atlas (assets/facades/<building>.jpg) and record
     the pixel rectangles in assets/facades/index.json for build_map.py.

Camera assumptions (calibrated by eye on 347 Rue Saint-Martin): lens 2.4 m above the ground
(car-roof rig), pitch -3° for the campaign frames, 0 for the rest, no roll, flat ground.

Usage:  python -I tools/build_facades.py [--only w56319987,...] [--ppm 40] [--pitch -3] [--cam-h 2.4]
        [--limit N] [--max-cands 10] [--no-download]
"""
import sys, json, math, pathlib, argparse, time
import numpy as np
from PIL import Image
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib, fetch_panoramax
from osm_lib import point_in_poly, poly_area, centroid, bbox_of, dist_point_seg, clean_ring, ring_touches

Image.MAX_IMAGE_PIXELS = None
ROOT = osm_lib.ROOT
FAC_DIR = ROOT / "assets" / "facades"
FAC_DIR.mkdir(parents=True, exist_ok=True)

ap = argparse.ArgumentParser()
ap.add_argument("--only", default="")
ap.add_argument("--ppm", type=float, default=90.0)
ap.add_argument("--feather", type=float, default=0.35, help="seam blend width in metres (0 = hard cuts)")
ap.add_argument("--no-match", action="store_true", help="skip the per-photo exposure matching")
ap.add_argument("--no-mapillary", action="store_true", help="use the Panoramax photos only")
ap.add_argument("--pano-width", type=int, default=8192, help="the 12k panoramas are kept at this width on disk")
ap.add_argument("--pitch", type=float, default=3.3, help="campaign frame pitch (deg, + = up)")
ap.add_argument("--debug", action="store_true", help="save the source photos with the projected wall outline (data/_debug/)")
ap.add_argument("--cam-h", type=float, default=2.4)
ap.add_argument("--limit", type=int, default=0)
ap.add_argument("--max-cands", type=int, default=10)
ap.add_argument("--atlas", type=int, default=4096)
ap.add_argument("--no-download", action="store_true")
ap.add_argument("--min-coverage", type=float, default=0.45)
ap.add_argument("--shard", default="0/1", help="k/N: process every N-th building starting at k (run N processes in parallel)")
ap.add_argument("--force", action="store_true", help="redo buildings that already have an atlas")
ap.add_argument("--merge-only", action="store_true", help="just rebuild index.json from the per-building sidecars")
ap.add_argument("--repack", action="store_true", help="re-pack the existing atlases tightly (power-of-two width fitted to the content)")
args = ap.parse_args()

layout = json.loads((ROOT / "data" / "map" / "layout.json").read_text(encoding="utf-8"))
ZONE = [tuple(p) for p in layout["zone"]]
ZX0, ZY0, ZX1, ZY1 = bbox_of(ZONE)
MARGIN = layout["backdrop_margin"]
BX0, BY0, BX1, BY1 = ZX0 - MARGIN, ZY0 - MARGIN, ZX1 + MARGIN, ZY1 + MARGIN
osm = osm_lib.load()


def v_sub(a, b): return (a[0] - b[0], a[1] - b[1])
def v_len(a): return math.hypot(a[0], a[1])
def v_norm(a):
    L = v_len(a); return (a[0] / L, a[1] / L) if L > 1e-9 else (0.0, 0.0)


# ---------------------------------------------------------------- footprints (same filter as build_map)
buildings = []          # detailed ones to texture
rings = []              # every footprint in the backdrop box (occluders)
for b in osm["buildings"]:
    ring = clean_ring(b["pts"])
    if len(ring) < 3 or abs(poly_area(ring)) < 4 or not ring_touches(ring, BX0, BY0, BX1, BY1):
        continue
    rings.append(ring)
    if ring_touches(ring, ZX0 - 15, ZY0 - 15, ZX1 + 15, ZY1 + 15):
        buildings.append({"id": b["id"], "ring": ring, "h": b["height"], "c": centroid(ring)})
if args.only:
    keep = set(args.only.split(","))
    buildings = [b for b in buildings if b["id"] in keep]
buildings.sort(key=lambda b: (round(b["c"][0] / 60), b["c"][1]))       # spatial order: better photo cache hits
if args.limit:
    buildings = buildings[:args.limit]
SHARD_K, SHARD_N = (int(v) for v in args.shard.split("/"))
buildings = buildings[SHARD_K::SHARD_N]


def sidecar(bid):
    return FAC_DIR / f"bld_{bid}.json"


def merge_index():
    """index.json = every per-building sidecar (resumable, shard-safe)."""
    merged = {}
    for f in sorted(FAC_DIR.glob("bld_*.json")):
        try:
            merged.update(json.loads(f.read_text(encoding="utf-8")))
        except Exception as e:
            print("  bad sidecar", f.name, e)
    (FAC_DIR / "index.json").write_text(json.dumps(merged, indent=0), encoding="utf-8")
    return merged


def pack(sizes, max_w):
    """Shelf-pack (w, h) rectangles into a power-of-two-wide sheet fitted to the content. -> (W, H, rects)"""
    area = sum(w * h for w, h in sizes)
    W = 8
    while W < max(max(w for w, h in sizes), math.sqrt(area * 1.15)):
        W *= 2
    W = min(W, max_w)
    order = sorted(range(len(sizes)), key=lambda k: -sizes[k][1])
    rects = [None] * len(sizes)
    x = y = row_h = 0
    for k in order:
        w, h = sizes[k]
        if x + w > W:
            x = 0; y += row_h; row_h = 0
        rects[k] = (x, y, w, h); x += w; row_h = max(row_h, h)
    H = 8
    while H < y + row_h:
        H *= 2
    return W, H, rects


if args.merge_only:
    print(len(merge_index()), "buildings in index.json"); sys.exit(0)
if args.repack:
    n = 0
    for sc in sorted(FAC_DIR.glob("bld_*.json")):
        d = json.loads(sc.read_text(encoding="utf-8"))
        for bid, e in d.items():
            faces = list(e["faces"].items())
            if not faces:
                continue
            src = Image.open(ROOT / e["atlas"]).convert("RGB")
            sizes = [(f["x1"] - f["x0"], f["y1"] - f["y0"]) for _, f in faces]
            W, H, rects = pack(sizes, args.atlas)
            if (W, H) == (src.width, src.height):
                continue
            dst = Image.new("RGB", (W, H), (186, 176, 160))
            for (k, f), (x, y, w, h) in zip(faces, rects):
                dst.paste(src.crop((f["x0"], f["y0"], f["x1"], f["y1"])), (x, y))
                f.update({"x0": x, "y0": y, "x1": x + w, "y1": y + h})
            dst.save(ROOT / e["atlas"], quality=90)
            e["w"], e["h"] = W, H
            n += 1
        sc.write_text(json.dumps(d), encoding="utf-8")
    print(n, "atlases re-packed;", len(merge_index()), "buildings in index.json"); sys.exit(0)
if not args.force and not args.only:
    before = len(buildings)
    buildings = [b for b in buildings if not sidecar(b["id"]).exists()]
    print(f"shard {SHARD_K}/{SHARD_N}: {before - len(buildings)} buildings already done, {len(buildings)} to do")

CELL = 25.0
grid = {}
for ri, ring in enumerate(rings):
    x0, y0, x1, y1 = bbox_of(ring)
    for i in range(int(math.floor(x0 / CELL)), int(math.floor(x1 / CELL)) + 1):
        for j in range(int(math.floor(y0 / CELL)), int(math.floor(y1 / CELL)) + 1):
            grid.setdefault((i, j), []).append(ri)


def rings_near(x0, y0, x1, y1):
    out = set()
    for i in range(int(math.floor(x0 / CELL)), int(math.floor(x1 / CELL)) + 1):
        for j in range(int(math.floor(y0 / CELL)), int(math.floor(y1 / CELL)) + 1):
            out.update(grid.get((i, j), ()))
    return out


def seg_cross(a, b, c, d):
    def orient(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    o1, o2, o3, o4 = orient(a, b, c), orient(a, b, d), orient(c, d, a), orient(c, d, b)
    return (o1 > 0) != (o2 > 0) and (o3 > 0) != (o4 > 0)


def blocked(cam, pt, skip_ring=None, skip_edge=None):
    """True if the ground segment cam->pt crosses a footprint edge (other than the wall itself)."""
    x0, y0 = min(cam[0], pt[0]), min(cam[1], pt[1]); x1, y1 = max(cam[0], pt[0]), max(cam[1], pt[1])
    for ri in rings_near(x0, y0, x1, y1):
        ring = rings[ri]; n = len(ring)
        for k in range(n):
            if ri == skip_ring and k == skip_edge:
                continue
            if seg_cross(cam, pt, ring[k], ring[(k + 1) % n]):
                return True
    return False


def inside_any(p, skip_ring=None):
    for ri in rings_near(p[0], p[1], p[0], p[1]):
        if ri != skip_ring and point_in_poly(p, rings[ri]):
            return True
    return False


ring_index = {id(r): i for i, r in enumerate(rings)}

# ---------------------------------------------------------------- photos
pics = [p for p in fetch_panoramax.load_index() if BX0 - 80 <= p["x"] <= BX1 + 80 and BY0 - 80 <= p["y"] <= BY1 + 80
        and (p["campaign"] or p["proj"] == "pinhole")]      # contributors' panoramas are not levelled (bike rigs): skip
# second source: Mapillary (volunteer phones / dashcams / 360s; SfM-corrected position + rotation => known pitch)
import fetch_mapillary
if fetch_mapillary.INDEX.exists() and not args.no_mapillary:
    mly = [p for p in json.loads(fetch_mapillary.INDEX.read_text(encoding="utf-8"))["pictures"]
           if BX0 - 80 <= p["x"] <= BX1 + 80 and BY0 - 80 <= p["y"] <= BY1 + 80 and fetch_mapillary.image_path(p).exists()]
    print(f"  + {len(mly)} Mapillary pictures")
    pics += mly
# contributors' frames (phones, action cams: unknown pitch) are a last resort only; Mapillary's known-pitch frames sit in between
WEIGHT = {True: 1.0, False: 0.2}
MLY_WEIGHT = 0.45


def weight(pic):
    return MLY_WEIGHT if pic.get("src") == "mapillary" else WEIGHT[bool(pic["campaign"])]
PX = np.array([p["x"] for p in pics]); PY = np.array([p["y"] for p in pics])
print(f"{len(buildings)} buildings to texture, {len(rings)} footprints, {len(pics)} photos in range")

_cache = {}         # pic id -> np.uint8 (H, W, 3)
_cache_order = []
CACHE_MAX = 6


def load_image(pic):
    pid = pic["id"]
    if pid in _cache:
        return _cache[pid]
    path = fetch_mapillary.image_path(pic) if pic.get("src") == "mapillary" else fetch_panoramax.image_path(pic)
    im = Image.open(path)
    PW = args.pano_width
    if im.width > PW:                        # the 12k panoramas: keep a smaller copy on disk (memory)
        small = path.with_name(path.stem + f"_{PW // 1024}k.jpg")
        if not small.exists():
            im.convert("RGB").resize((PW, im.height * PW // im.width), Image.LANCZOS).save(small, quality=92)
        im = Image.open(small)
    arr = np.asarray(im.convert("RGB"))
    _cache[pid] = arr; _cache_order.append(pid)
    while len(_cache_order) > CACHE_MAX:
        old = _cache_order.pop(0); _cache.pop(old, None)
    return arr


def sample_bilinear(img, px, py):
    H, W, _ = img.shape
    x0 = np.clip(np.floor(px).astype(np.int32), 0, W - 2); y0 = np.clip(np.floor(py).astype(np.int32), 0, H - 2)
    fx = np.clip(px - x0, 0, 1)[..., None]; fy = np.clip(py - y0, 0, 1)[..., None]
    a = img[y0, x0].astype(np.float32); b = img[y0, x0 + 1].astype(np.float32)
    c = img[y0 + 1, x0].astype(np.float32); d = img[y0 + 1, x0 + 1].astype(np.float32)
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def box_blur(m, r):
    """Separable box blur (radius r texels) of a float32 2-D array, via cumulative sums; applied twice ~ Gaussian."""
    if r <= 0:
        return m
    for _ in range(2):
        for axis in (0, 1):
            n = m.shape[axis]
            c = np.cumsum(np.concatenate([np.zeros_like(np.take(m, [0], axis=axis)), m], axis=axis), axis=axis)
            hi = np.clip(np.arange(n) + r + 1, 0, n); lo = np.clip(np.arange(n) - r, 0, n)
            m = (np.take(c, hi, axis=axis) - np.take(c, lo, axis=axis)) / (hi - lo).reshape([-1 if a == axis else 1 for a in (0, 1)])
    return m


_rot_cache = {}


def rot_matrix(rvec):
    """Axis-angle -> 3x3 rotation (Rodrigues), cached per picture."""
    key = tuple(rvec)
    if key in _rot_cache:
        return _rot_cache[key]
    v = np.array(rvec, np.float64); th = float(np.linalg.norm(v))
    if th < 1e-12:
        R = np.eye(3)
    else:
        k = v / th; K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
        R = np.eye(3) * math.cos(th) + math.sin(th) * K + (1 - math.cos(th)) * np.outer(k, k)
    _rot_cache[key] = R
    return R


def project(pic, dims, X, Y, Z, nrm):
    """Per-texel image coordinates + quality score (0 = cannot see). X, Y, Z world metres, dims = (W, H)."""
    W, H = dims
    cx, cy, cz = pic["x"], pic["y"], pic.get("cam_h", args.cam_h)
    vx, vy, vz = X - cx, Y - cy, Z - cz
    dist = np.sqrt(vx * vx + vy * vy + vz * vz) + 1e-6
    cos_inc = -(vx * nrm[0] + vy * nrm[1]) / dist                   # incidence on the wall
    if pic.get("rot"):
        # Mapillary: full OpenSfM orientation (world ENU -> camera: x right, y down, z forward), roll included
        R = rot_matrix(pic["rot"])
        xc = R[0, 0] * vx + R[0, 1] * vy + R[0, 2] * vz
        yc = R[1, 0] * vx + R[1, 1] * vy + R[1, 2] * vz
        zc = R[2, 0] * vx + R[2, 1] * vy + R[2, 2] * vz
        if pic["proj"] == "equirect":
            yaw = np.arctan2(xc, zc)
            pitch = np.arctan2(-yc, np.sqrt(xc * xc + zc * zc))
            px = (yaw / (2 * math.pi) + 0.5) * W
            py = (0.5 - pitch / math.pi) * H
            dens = (W / (2 * math.pi)) / dist
            edge = np.ones_like(px)
            valid = (cos_inc > 0.2) & (py > 0) & (py < H - 1)
        else:
            F = pic["focal"] * max(W, H)
            with np.errstate(divide="ignore", invalid="ignore"):
                px = W / 2 + F * xc / zc
                py = H / 2 + F * yc / zc
            dens = F / dist
            ex = np.abs(px - W / 2) / (W / 2); ey = np.abs(py - H / 2) / (H / 2)
            edge = 1.0 - 0.35 * np.clip(np.maximum(ex, ey), 0, 1) ** 2
            valid = (zc > 0.5) & (cos_inc > 0.2) & (px > 0) & (px < W - 1) & (py > 0) & (py < H - 1)
        score = np.where(valid, dens * cos_inc * edge * weight(pic), 0.0)
        return px, py, score
    az = math.radians(pic["az"])
    fwd = (math.sin(az), math.cos(az)); right = (math.cos(az), -math.sin(az))
    xr = vx * right[0] + vy * right[1]
    xf = vx * fwd[0] + vy * fwd[1]
    if pic["proj"] == "equirect":
        yaw = np.arctan2(xr, xf)
        pitch = np.arctan2(vz, np.sqrt(xr * xr + xf * xf))
        px = (yaw / (2 * math.pi) + 0.5) * W
        py = (0.5 - pitch / math.pi) * H
        dens = (W / (2 * math.pi)) / dist
        edge = np.ones_like(px)
        valid = (cos_inc > 0.2) & (py > 0) & (py < H - 1)
    else:
        pitch = math.radians(pic["pitch"] if "pitch" in pic else (args.pitch if pic["campaign"] else 0.0))
        xf2 = xf * math.cos(pitch) + vz * math.sin(pitch)
        xu = -xf * math.sin(pitch) + vz * math.cos(pitch)
        F = (W / 2) / math.tan(math.radians(pic["fov"]) / 2)
        with np.errstate(divide="ignore", invalid="ignore"):
            px = W / 2 + F * xr / xf2
            py = H / 2 - F * xu / xf2
        dens = F / dist
        ex = np.abs(px - W / 2) / (W / 2); ey = np.abs(py - H / 2) / (H / 2)
        edge = 1.0 - 0.35 * np.clip(np.maximum(ex, ey), 0, 1) ** 2
        valid = (xf2 > 0.5) & (cos_inc > 0.2) & (px > 0) & (px < W - 1) & (py > 0) & (py < H - 1)
    score = dens * cos_inc * edge * weight(pic)
    score = np.where(valid, score, 0.0)
    return px, py, score


def wall_candidates(b, i, p, q, d, nrm, L, h):
    ri = ring_index[id(b["ring"])]
    mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
    dx = PX - mid[0]; dy = PY - mid[1]
    dist = np.sqrt(dx * dx + dy * dy)
    side = dx * nrm[0] + dy * nrm[1]
    sel = np.where((dist < 75) & (side > 2.0) & (side / np.maximum(dist, 1e-6) > 0.2))[0]
    scored = []
    for k in sel:
        pic = pics[k]
        dd = float(dist[k]); cosang = float(side[k]) / dd
        if pic["proj"] == "pinhole":
            # bearing camera -> wall centre must be inside the lens
            bearing = math.degrees(math.atan2(mid[0] - pic["x"], mid[1] - pic["y"]))
            diff = (bearing - pic["az"] + 180) % 360 - 180
            if abs(diff) > pic["fov"] / 2 + 8:
                continue
            prior = cosang / max(dd, 4.0) * weight(pic)
        else:
            prior = cosang / max(dd, 4.0) * weight(pic)
        scored.append((prior, k))
    scored.sort(reverse=True)
    scored = scored[:120]
    ins = min(0.4, L * 0.2)
    probes = [(p[0] + d[0] * ins + nrm[0] * 0.3, p[1] + d[1] * ins + nrm[1] * 0.3),
              (q[0] - d[0] * ins + nrm[0] * 0.3, q[1] - d[1] * ins + nrm[1] * 0.3),
              (mid[0] + nrm[0] * 0.3, mid[1] + nrm[1] * 0.3)]
    # coarse 7 x 7 grid over the wall: greedily add the photo that improves the most of it
    gu = (np.arange(7) + 0.5) / 7 * L; gz = (np.arange(7) + 0.5) / 7 * h
    GX = p[0] + d[0] * gu[None, :] + np.zeros((7, 1)); GY = p[1] + d[1] * gu[None, :] + np.zeros((7, 1)); GZ = gz[:, None] + np.zeros((1, 7))
    cand_scores = []
    for prior, k in scored:
        pic = pics[k]
        if not pic.get("w") or not pic.get("h"):
            continue
        _, _, sc = project(pic, (pic["w"], pic["h"]), GX, GY, GZ, nrm)
        if sc.max() > 0:
            cand_scores.append((k, sc))
    best = np.zeros((7, 7)); out = []; checked = set()
    while len(out) < args.max_cands and cand_scores:
        gains = [float(np.maximum(0.0, sc - best * 1.15).sum()) for k, sc in cand_scores]
        j = int(np.argmax(gains))
        if gains[j] <= 1e-6:
            break
        k, sc = cand_scores.pop(j)
        pic = pics[k]; cam = (pic["x"], pic["y"])
        if any(blocked(cam, pr, ri, i) for pr in probes):
            continue
        out.append(pic); best = np.maximum(best, sc)
    return out


# ---------------------------------------------------------------- per building
index = {}
t0 = time.time()
n_faces = n_pics_used = 0
sheet = []
for bi, b in enumerate(buildings):
    ring, h = b["ring"], b["h"]; n = len(ring)
    ri = ring_index[id(ring)]
    faces = []
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        L = v_len(v_sub(q, p))
        if L < 2.5:
            continue
        d = v_norm(v_sub(q, p)); nrm = (d[1], -d[0])
        mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
        if inside_any((mid[0] + nrm[0] * 0.8, mid[1] + nrm[1] * 0.8), ri):
            continue                                   # shared wall / courtyard side
        cands = wall_candidates(b, i, p, q, d, nrm, L, h)
        if cands:
            faces.append({"i": i, "p": p, "q": q, "d": d, "nrm": nrm, "L": L, "cands": cands})
    if not faces:
        continue
    if not args.no_download:
        need = {c["id"]: c for f in faces for c in f["cands"] if c.get("src") != "mapillary"}
        fetch_panoramax.ensure_images(list(need.values()))
    # texture size: fit the atlas (the px/m is reduced until the walls fit a 4096 x 4096 sheet)
    ppm = args.ppm
    while True:
        sizes = [(max(8, int(math.ceil(f["L"] * ppm))), max(8, int(math.ceil(h * ppm)))) for f in faces]
        atlas_w, atlas_h, rects = pack(sizes, args.atlas)
        if atlas_h <= args.atlas and max(w for w, hh in sizes) <= args.atlas:
            break
        ppm *= 0.85
    atlas = np.zeros((atlas_h, atlas_w, 3), np.uint8)
    atlas[:] = (186, 176, 160)
    entries = {}
    for f, (x, y, w, hh) in zip(faces, rects):
        us = (np.arange(w) + 0.5) / ppm
        zs = h - (np.arange(hh) + 0.5) / ppm                     # row 0 = top of the wall
        X = f["p"][0] + f["d"][0] * us[None, :] + np.zeros((hh, 1))
        Y = f["p"][1] + f["d"][1] * us[None, :] + np.zeros((hh, 1))
        Z = zs[:, None] + np.zeros((1, w))
        best = np.zeros((hh, w), np.float32)
        label = np.full((hh, w), -1, np.int16)
        out = np.zeros((hh, w, 3), np.float32)
        used = set()
        scores = []                                                # (candidate index, score map) for the blend pass
        for ci, pic in enumerate(f["cands"]):
            try:
                img = load_image(pic)
            except Exception as e:
                print("  cannot read", pic["id"], e); continue
            px, py, score = project(pic, (img.shape[1], img.shape[0]), X, Y, Z, f["nrm"])
            if args.debug:
                from PIL import ImageDraw
                dbg = ROOT / "data" / "_debug"; dbg.mkdir(exist_ok=True)
                sc = 1600 / img.shape[1]
                dim = Image.fromarray(img).resize((1600, int(img.shape[0] * sc)))
                dr = ImageDraw.Draw(dim)
                for (r0, c0), (r1, c1) in [((hh - 1, 0), (hh - 1, w - 1)), ((hh - 1, w - 1), (0, w - 1)), ((0, w - 1), (0, 0)), ((0, 0), (hh - 1, 0))]:
                    if score[r0, c0] > 0 or score[r1, c1] > 0:
                        dr.line([(px[r0, c0] * sc, py[r0, c0] * sc), (px[r1, c1] * sc, py[r1, c1] * sc)], fill=(255, 40, 40), width=3)
                for rr in range(0, hh, max(1, int(3.0 * ppm))):      # one line per floor (3 m)
                    pts = [(px[rr, cc] * sc, py[rr, cc] * sc) for cc in range(0, w, max(1, w // 8)) if score[rr, cc] > 0]
                    if len(pts) > 1:
                        dr.line(pts, fill=(40, 255, 40), width=1)
                dim.save(dbg / f"{b['id']}_{f['i']}_{pic['id'][:8]}_{'360' if pic['proj'] == 'equirect' else 'cam'}.jpg", quality=80)
            better = score > best * 1.4                            # hysteresis: a new photo must be clearly better (fewer seams)
            if not better.any():
                continue
            best[better] = score[better]; label[better] = ci
            scores.append((ci, score))
        cov = float((best > 0).mean())
        if cov >= args.min_coverage and scores:
            # ---- second pass: match every photo's exposure to the dominant one, then blend with feathered seams
            owners = [ci for ci, sc in scores if (label == ci).any()]
            areas = {ci: int((label == ci).sum()) for ci in owners}
            ref = max(owners, key=lambda ci: areas[ci])
            score_of = dict(scores)
            r = int(round(args.feather * ppm))
            gains = {ci: np.ones(3, np.float32) for ci in owners}
            if not args.no_match and len(owners) > 1:
                st = 4                                                 # subsampled grid for the statistics
                Xs, Ys, Zs = X[::st, ::st], Y[::st, ::st], Z[::st, ::st]
                ref_pic = f["cands"][ref]; ref_img = load_image(ref_pic)
                rpx, rpy, rsc = project(ref_pic, (ref_img.shape[1], ref_img.shape[0]), Xs, Ys, Zs, f["nrm"])
                rcols = sample_bilinear(ref_img, np.where(rsc > 0, rpx, 1.0), np.where(rsc > 0, rpy, 1.0))
                for ci in owners:
                    if ci == ref:
                        continue
                    pic = f["cands"][ci]; img = load_image(pic)
                    cpx, cpy, csc = project(pic, (img.shape[1], img.shape[0]), Xs, Ys, Zs, f["nrm"])
                    both = (rsc > 0) & (csc > 0)
                    if both.sum() < 40:
                        continue
                    ccols = sample_bilinear(img, np.where(both, cpx, 1.0), np.where(both, cpy, 1.0))
                    gains[ci] = np.clip(rcols[both].mean(0) / np.maximum(ccols[both].mean(0), 1.0), 0.7, 1.45).astype(np.float32)
            acc = np.zeros((hh, w, 3), np.float32); norm = np.zeros((hh, w), np.float32)
            for ci in owners:
                pic = f["cands"][ci]; img = load_image(pic)
                px, py, _ = project(pic, (img.shape[1], img.shape[0]), X, Y, Z, f["nrm"])
                valid = score_of[ci] > 0
                own = (label == ci).astype(np.float32)
                wgt = (box_blur(own, r) if r > 0 else own) * valid
                if not (wgt > 0).any():
                    continue
                cols = sample_bilinear(img, np.where(valid, px, 1.0), np.where(valid, py, 1.0))
                acc += wgt[..., None] * (cols * gains[ci][None, None, :])
                norm += wgt
                used.add(pic["id"])
            ok = norm > 1e-6
            out[ok] = acc[ok] / norm[ok][:, None]
            best = np.where(ok, best, 0.0)
            del acc, norm, scores
        cov = float((best > 0).mean())
        if cov < args.min_coverage:
            continue
        if cov < 1.0:                                              # fill holes with the column average
            miss = best <= 0
            colmean = np.where((~miss).any(0)[:, None], out.sum(0) / np.maximum((~miss).sum(0), 1)[:, None], np.array([186, 176, 160.0]))
            out[miss] = colmean[np.where(miss)[1]]
        atlas[y:y + hh, x:x + w] = np.clip(out, 0, 255).astype(np.uint8)
        entries[str(f["i"])] = {"x0": x, "y0": y, "x1": x + w, "y1": y + hh, "coverage": round(cov, 3), "photos": len(used)}
        n_faces += 1; n_pics_used += len(used)
    if not entries:
        sidecar(b["id"]).write_text("{}", encoding="utf-8")       # remember: nothing usable here
        continue
    name = f"bld_{b['id']}.jpg"
    Image.fromarray(atlas).save(FAC_DIR / name, quality=90)
    index[b["id"]] = {"atlas": f"assets/facades/{name}", "w": atlas_w, "h": atlas_h, "ppm": round(ppm, 2), "height": h, "faces": entries}
    sidecar(b["id"]).write_text(json.dumps({b["id"]: index[b["id"]]}), encoding="utf-8")
    if len(sheet) < 12:
        sheet.append(Image.fromarray(atlas).resize((512, max(8, 512 * atlas_h // atlas_w))))
    if bi % 10 == 0:
        print(f"  [{bi + 1}/{len(buildings)}] {b['id']}: {len(entries)} walls, ppm {ppm:.0f}, {time.time() - t0:.0f}s")

if not args.only and not args.limit:
    print(len(merge_index()), "buildings in", FAC_DIR / "index.json")
else:
    (FAC_DIR / "index_partial.json").write_text(json.dumps(index, indent=0), encoding="utf-8")
if sheet:
    Wc = 512 * 3; rows = (len(sheet) + 2) // 3
    hs = [max(im.height for im in sheet[r * 3:(r + 1) * 3]) for r in range(rows)]
    cs = Image.new("RGB", (Wc, sum(hs)), (30, 30, 30))
    yy = 0
    for r in range(rows):
        for c in range(3):
            k = r * 3 + c
            if k < len(sheet):
                cs.paste(sheet[k], (c * 512, yy))
        yy += hs[r]
    cs.save(ROOT / "data" / "preview_facades.jpg", quality=80)
print(f"{len(index)} buildings textured, {n_faces} walls, {n_pics_used} photo uses, {time.time() - t0:.0f}s")
