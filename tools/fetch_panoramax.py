"""Inventory (and on demand download) the street-level photos covering the map zone.

Source: Panoramax (https://panoramax.fr) — the French open street-imagery platform (IGN + OSM France).
The neighbourhood is covered by the City of Paris survey ("Immergis pour la Ville de Paris", 2024-25,
imajbox 3TX+ 95° frames every few metres + imajbox 360 HD panoramas, Etalab 2.0 licence) and by
contributors' uploads (CC-BY-SA).

Outputs:
    data/raw/panoramax/index.json   one record per picture: position (map metres), heading, lens, urls
    data/raw/panoramax/img/<id>.jpg full-resolution pictures, fetched lazily by build_facades.py
Usage:  python -I tools/fetch_panoramax.py          (re)build the index
"""
import os, sys, json, math, pathlib, subprocess, urllib.request, concurrent.futures
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

ROOT = osm_lib.ROOT
PMX = ROOT / "data" / "raw" / "panoramax"
IMG = PMX / "img"
INDEX = PMX / "index.json"
API = "https://api.panoramax.xyz/api/search"
CAMPAIGN_ARTIST = "Immergis pour la Ville de Paris"


def _dropbox_ignore(path):
    """Keep the photo cache out of Dropbox sync (it is re-downloadable, ~1 GB)."""
    try:
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        f'Set-Content -Path "{path}" -Stream com.dropbox.ignored -Value 1'],
                       capture_output=True, timeout=30)
    except Exception:
        pass


def zone_bbox(margin=60.0):
    layout = json.loads((ROOT / "data" / "map" / "layout.json").read_text(encoding="utf-8"))
    xs = [p[0] for p in layout["zone"]]; ys = [p[1] for p in layout["zone"]]
    x0, y0, x1, y1 = min(xs) - margin, min(ys) - margin, max(xs) + margin, max(ys) + margin
    return (osm_lib.ORIGIN_LON + x0 / osm_lib.M_PER_DEG_LON, osm_lib.ORIGIN_LAT + y0 / osm_lib.M_PER_DEG_LAT,
            osm_lib.ORIGIN_LON + x1 / osm_lib.M_PER_DEG_LON, osm_lib.ORIGIN_LAT + y1 / osm_lib.M_PER_DEG_LAT)


def fetch_index():
    lon0, lat0, lon1, lat1 = zone_bbox()
    n = 6                      # the API caps a search at 1000 items: tile the bbox
    feats = {}
    for i in range(n):
        for j in range(n):
            bb = (lon0 + (lon1 - lon0) * i / n, lat0 + (lat1 - lat0) * j / n,
                  lon0 + (lon1 - lon0) * (i + 1) / n, lat0 + (lat1 - lat0) * (j + 1) / n)
            url = API + "?bbox=%.6f,%.6f,%.6f,%.6f&limit=1000" % bb
            with urllib.request.urlopen(url, timeout=180) as r:
                d = json.load(r)
            fs = d.get("features", [])
            if len(fs) >= 1000:
                print("  WARN cell", i, j, "hit the 1000 cap")
            for f in fs:
                feats[f["id"]] = f
            print(f"  cell {i},{j}: {len(fs)} pictures")
    recs = []
    for f in feats.values():
        p = f["properties"]; io = p.get("pers:interior_orientation") or {}
        lon, lat = f["geometry"]["coordinates"][:2]
        az = p.get("view:azimuth")
        if az is None:
            continue
        fov = io.get("field_of_view")
        dims = io.get("sensor_array_dimensions") or [None, None]
        proj = "equirect" if (fov == 360 or p.get("exif", {}).get("Xmp.GPano.ProjectionType") == "equirectangular") else "pinhole"
        if proj == "pinhole" and not fov:
            continue
        hd = (f.get("assets") or {}).get("hd", {}).get("href") or p.get("geovisio:image")
        if not hd:
            continue
        x, y = osm_lib.project(lat, lon)
        recs.append({
            "id": f["id"], "lon": lon, "lat": lat, "x": round(x, 2), "y": round(y, 2),
            "az": float(az), "fov": float(fov) if fov else None, "w": dims[0], "h": dims[1],
            "proj": proj, "campaign": p.get("exif", {}).get("Exif.Image.Artist") == CAMPAIGN_ARTIST,
            "camera": io.get("camera_model"), "seq": f.get("collection"), "date": p.get("datetime", "")[:10],
            "accuracy": p.get("quality:horizontal_accuracy"), "hd": hd,
        })
    PMX.mkdir(parents=True, exist_ok=True)
    INDEX.write_text(json.dumps({"bbox_lonlat": [lon0, lat0, lon1, lat1], "pictures": recs}), encoding="utf-8")
    camp = sum(1 for r in recs if r["campaign"]); eq = sum(1 for r in recs if r["proj"] == "equirect")
    print(f"wrote {INDEX}: {len(recs)} pictures ({camp} City of Paris campaign, {eq} panoramas)")
    return recs


def load_index():
    return json.loads(INDEX.read_text(encoding="utf-8"))["pictures"]


def image_path(pic):
    return IMG / f"{pic['id']}.jpg"


def ensure_images(pics, workers=8):
    """Download the full-resolution files that are not cached yet. Returns the number fetched."""
    IMG.mkdir(parents=True, exist_ok=True)
    _dropbox_ignore(IMG)
    todo = [p for p in pics if not image_path(p).exists()]
    if not todo:
        return 0

    def get(p):
        dst = image_path(p); tmp = dst.with_suffix(f".part{os.getpid()}")
        for attempt in range(3):
            try:
                urllib.request.urlretrieve(p["hd"], tmp)
                tmp.replace(dst)
                return True
            except Exception as e:
                err = e
                if dst.exists():                 # another shard fetched it meanwhile
                    return True
        print("  download failed", p["id"], err)
        return False

    done = 0
    with concurrent.futures.ThreadPoolExecutor(workers) as ex:
        for ok in ex.map(get, todo):
            done += 1 if ok else 0
            if done % 100 == 0:
                print(f"  downloaded {done}/{len(todo)}")
    return done


if __name__ == "__main__":
    fetch_index()
