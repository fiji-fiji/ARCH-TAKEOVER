"""Download the IGN high-resolution aerial orthophoto for the map box (the real ground).

Source: IGN Géoplateforme WMS, layer HR.ORTHOIMAGERY.ORTHOPHOTOS (open licence, no key, ~20 cm/px;
Paris is covered at higher resolution). Fetched as 2048 px tiles and stitched.
Output: assets/ortho/ground.jpg + ground.json (map-metre bbox, px/m)  and data/preview_ortho.jpg
Usage:  python -I tools/fetch_ortho.py [--ppm 8]
"""
import io, sys, json, math, pathlib, argparse, urllib.request, urllib.parse
from PIL import Image
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

Image.MAX_IMAGE_PIXELS = None
ROOT = osm_lib.ROOT
OUT = ROOT / "assets" / "ortho"
OUT.mkdir(parents=True, exist_ok=True)

ap = argparse.ArgumentParser()
ap.add_argument("--ppm", type=float, default=8.0, help="pixels per metre of the stitched image")
ap.add_argument("--tile", type=int, default=2048)
args = ap.parse_args()

layout = json.loads((ROOT / "data" / "map" / "layout.json").read_text(encoding="utf-8"))
xs = [p[0] for p in layout["zone"]]; ys = [p[1] for p in layout["zone"]]
m = layout["backdrop_margin"]
BX0, BY0, BX1, BY1 = min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m
W = int(math.ceil((BX1 - BX0) * args.ppm)); H = int(math.ceil((BY1 - BY0) * args.ppm))
print(f"box x[{BX0},{BX1}] y[{BY0},{BY1}] m -> {W} x {H} px")


def lonlat(x, y):
    return (osm_lib.ORIGIN_LON + x / osm_lib.M_PER_DEG_LON, osm_lib.ORIGIN_LAT + y / osm_lib.M_PER_DEG_LAT)


sheet = Image.new("RGB", (W, H))
nx, ny = math.ceil(W / args.tile), math.ceil(H / args.tile)
for j in range(ny):
    for i in range(nx):
        px0, py0 = i * args.tile, j * args.tile
        px1, py1 = min(W, px0 + args.tile), min(H, py0 + args.tile)
        x0, x1 = BX0 + px0 / args.ppm, BX0 + px1 / args.ppm
        y1, y0 = BY1 - py0 / args.ppm, BY1 - py1 / args.ppm          # row 0 = north
        lon0, lat0 = lonlat(x0, y0); lon1, lat1 = lonlat(x1, y1)
        q = {"SERVICE": "WMS", "VERSION": "1.3.0", "REQUEST": "GetMap", "LAYERS": "HR.ORTHOIMAGERY.ORTHOPHOTOS",
             "STYLES": "", "CRS": "EPSG:4326", "BBOX": f"{lat0:.7f},{lon0:.7f},{lat1:.7f},{lon1:.7f}",
             "WIDTH": str(px1 - px0), "HEIGHT": str(py1 - py0), "FORMAT": "image/jpeg"}
        url = "https://data.geopf.fr/wms-r?" + urllib.parse.urlencode(q)
        for attempt in range(3):                      # in memory: Dropbox locks fresh files in the workspace
            try:
                with urllib.request.urlopen(url, timeout=180) as r:
                    data = r.read()
                break
            except Exception as e:
                err = e
        else:
            raise SystemExit(f"tile {i},{j} failed: {err}")
        with Image.open(io.BytesIO(data)) as tile:
            sheet.paste(tile.convert("RGB"), (px0, py0))
        print(f"  tile {i},{j} ok")
sheet.save(OUT / "ground.jpg", quality=90)
(OUT / "ground.json").write_text(json.dumps({"bbox": [BX0, BY0, BX1, BY1], "ppm": args.ppm, "w": W, "h": H,
                                             "source": "IGN HR.ORTHOIMAGERY.ORTHOPHOTOS (Licence Ouverte)"}), encoding="utf-8")
sheet.resize((W // 4, H // 4)).save(ROOT / "data" / "preview_ortho.jpg", quality=80)
print("wrote", OUT / "ground.jpg", "and data/preview_ortho.jpg")
