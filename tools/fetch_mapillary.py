"""Second street-imagery source: Mapillary (Meta; volunteer phone / dashcam / 360 photos, CC BY-SA 4.0).

Fills the streets and upper floors the City of Paris survey (Panoramax) does not cover. Needs a free client token in
data/raw/mapillary_token.txt (https://www.mapillary.com/dashboard/developers, not committed).

Outputs (same record schema as data/raw/panoramax/index.json, plus "src": "mapillary", "pitch", "cam_h", "path"):
    data/raw/mapillary/index.json
    data/raw/mapillary/img/<id>.jpg      2048 px frames, full-size panoramas (the signed URLs expire: fetched here)
Usage:  python -I tools/fetch_mapillary.py [--margin 60] [--workers 8]
"""
import os, sys, json, math, pathlib, argparse, urllib.request, urllib.parse, concurrent.futures, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib, fetch_panoramax

ROOT = osm_lib.ROOT
MLY = ROOT / "data" / "raw" / "mapillary"
IMG = MLY / "img"
INDEX = MLY / "index.json"
TOKEN_FILE = ROOT / "data" / "raw" / "mapillary_token.txt"
API = "https://graph.mapillary.com/images"
FIELDS = ("id,captured_at,compass_angle,computed_compass_angle,computed_geometry,geometry,camera_type,camera_parameters,"
          "width,height,is_pano,sequence,computed_altitude,computed_rotation,thumb_2048_url,thumb_original_url,make,model")


def token():
    if not TOKEN_FILE.exists():
        sys.exit(f"no Mapillary token: put the client token (MLY|...) in {TOKEN_FILE}")
    return TOKEN_FILE.read_text(encoding="utf-8").strip()


def api(params):
    url = API + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "paris-map-builder"})
    for attempt in range(4):
        try:
            return json.load(urllib.request.urlopen(req, timeout=90))
        except Exception as e:
            err = e; time.sleep(2 + attempt * 3)
    raise err


def rot_to_dir(rvec):
    """OpenSfM axis-angle (world -> camera, camera z forward, y down) -> camera forward direction in the world frame."""
    x, y, z = rvec
    th = math.sqrt(x * x + y * y + z * z)
    if th < 1e-9:
        return (0.0, 0.0, 1.0)
    kx, ky, kz = x / th, y / th, z / th
    c, s = math.cos(th), math.sin(th)
    # R = c I + s [k]x + (1-c) k k^T ; forward_world = R^T (0,0,1) = third ROW of R
    r20 = (1 - c) * kz * kx - s * ky
    r21 = (1 - c) * kz * ky + s * kx
    r22 = c + (1 - c) * kz * kz
    return (r20, r21, r22)


def fetch_index(margin=60.0):
    tok = token()
    lon0, lat0, lon1, lat1 = fetch_panoramax.zone_bbox(margin)
    recs, seen = [], set()
    n = 2                                                        # 2 x 2 tiles: each stays under the 2000-item cap
    for i in range(n):
        for j in range(n):
            bb = (lon0 + (lon1 - lon0) * i / n, lat0 + (lat1 - lat0) * j / n, lon0 + (lon1 - lon0) * (i + 1) / n, lat0 + (lat1 - lat0) * (j + 1) / n)
            data = api({"access_token": tok, "fields": FIELDS, "bbox": ",".join(f"{v:.7f}" for v in bb), "limit": 2000})
            items = data.get("data", [])
            if len(items) >= 2000:
                print("  WARN tile", i, j, "hit the 2000 cap: raise the tiling")
            for it in items:
                if it["id"] in seen:
                    continue
                seen.add(it["id"])
                rec = to_record(it)
                if rec:
                    recs.append(rec)
    MLY.mkdir(parents=True, exist_ok=True)
    INDEX.write_text(json.dumps({"source": "Mapillary (CC BY-SA 4.0)", "pictures": recs}, indent=0), encoding="utf-8")
    import collections
    print(f"{len(recs)} usable Mapillary pictures ({collections.Counter(r['proj'] for r in recs)}) -> {INDEX}")
    return recs


def to_record(it):
    ct = it.get("camera_type")
    w, h = it.get("width"), it.get("height")
    if not w or not h or not it.get("computed_rotation"):
        return None
    geom = it.get("computed_geometry") or it.get("geometry")
    if not geom:
        return None
    lon, lat = geom["coordinates"]
    x, y = osm_lib.project(lat, lon)
    fwd = rot_to_dir(it["computed_rotation"])
    az = math.degrees(math.atan2(fwd[0], fwd[1])) % 360          # heading from the SfM rotation (x east, y north)
    pitch = math.degrees(math.atan2(fwd[2], math.hypot(fwd[0], fwd[1])))
    if abs(pitch) > 35:
        return None                                              # pointing at the ground / sky
    if ct in ("spherical", "equirectangular") or it.get("is_pano"):
        proj, fov, url = "equirect", 360.0, it.get("thumb_original_url") or it.get("thumb_2048_url")
        az = float(it.get("computed_compass_angle", it.get("compass_angle", az)))
    elif ct == "perspective":
        params = it.get("camera_parameters") or []
        f = params[0] if params else 0.85
        focal_px = f * max(w, h)
        fov = math.degrees(2 * math.atan((w / 2) / focal_px))
        proj, url = "pinhole", it.get("thumb_2048_url") or it.get("thumb_original_url")
        if w > 2048:                                             # the 2048 thumbnail is what gets downloaded
            h = int(round(h * 2048 / w)); w = 2048
    else:
        return None                                              # fisheye: no model here
    if not url:
        return None
    cam_h = it.get("computed_altitude")
    cam_h = float(cam_h) if cam_h is not None and 0.8 <= cam_h <= 3.5 else 1.6
    return {"id": f"mly{it['id']}", "src": "mapillary", "lon": lon, "lat": lat, "x": round(x, 2), "y": round(y, 2),
            "az": round(az, 2), "pitch": round(pitch, 2), "fov": round(fov, 2), "w": w, "h": h, "proj": proj,
            "campaign": False, "camera": f"{it.get('make', '')} {it.get('model', '')}".strip(), "seq": it.get("sequence"),
            "date": it.get("captured_at"), "cam_h": round(cam_h, 2), "hd": url, "compass": it.get("computed_compass_angle"),
            "rot": [round(float(v), 6) for v in it["computed_rotation"]],          # full SfM orientation (heading, pitch, roll)
            "focal": round(float((it.get("camera_parameters") or [0.85])[0]), 6)}


def image_path(pic):
    return IMG / f"{pic['id']}.jpg"


def download(recs, workers=8):
    IMG.mkdir(parents=True, exist_ok=True)
    fetch_panoramax._dropbox_ignore(IMG)
    todo = [p for p in recs if not image_path(p).exists()]

    def get(p):
        dst = image_path(p); tmp = dst.with_suffix(f".part{os.getpid()}")
        for attempt in range(3):
            try:
                req = urllib.request.Request(p["hd"], headers={"User-Agent": "paris-map-builder"})
                with urllib.request.urlopen(req, timeout=120) as r, open(tmp, "wb") as f:
                    f.write(r.read())
                tmp.replace(dst)
                return True
            except Exception as e:
                err = e; time.sleep(1)
        print("  download failed", p["id"], err)
        return False

    done = 0
    with concurrent.futures.ThreadPoolExecutor(workers) as ex:
        for ok in ex.map(get, todo):
            done += 1 if ok else 0
            if done % 200 == 0:
                print(f"  downloaded {done}/{len(todo)}")
    print(f"{done} pictures downloaded, {len(recs) - len(todo)} were cached")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--margin", type=float, default=60.0)
    ap.add_argument("--workers", type=int, default=8)
    ap.add_argument("--no-download", action="store_true")
    args = ap.parse_args()
    recs = fetch_index(args.margin)
    # sanity: the heading from the SfM rotation should agree with Mapillary's own compass
    diffs = [abs((r["az"] - r["compass"] + 180) % 360 - 180) for r in recs if r["proj"] == "pinhole" and r.get("compass") is not None]
    if diffs:
        diffs.sort()
        print(f"heading check (rotation vs compass): median {diffs[len(diffs) // 2]:.1f} deg, 90th pct {diffs[int(len(diffs) * 0.9)]:.1f} deg")
    if not args.no_download:
        download(recs, args.workers)
