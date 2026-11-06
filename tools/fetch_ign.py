"""Download the IGN BD TOPO buildings (real measured heights) for the map zone.

Source: IGN Géoplateforme WFS, layer BDTOPO_V3:batiment (open licence, no key needed).
Output: data/raw/ign_batiment.json  (GeoJSON, EPSG:4326, coordinates [lon, lat, alt])
Usage:  python -I tools/fetch_ign.py
"""
import sys, json, pathlib, urllib.request, urllib.parse
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

ROOT = osm_lib.ROOT
OUT = ROOT / "data" / "raw" / "ign_batiment.json"

layout = json.loads((ROOT / "data" / "map" / "layout.json").read_text(encoding="utf-8"))
xs = [p[0] for p in layout["zone"]]; ys = [p[1] for p in layout["zone"]]
m = layout["backdrop_margin"] + 20
x0, y0, x1, y1 = min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m
lat0 = osm_lib.ORIGIN_LAT + y0 / osm_lib.M_PER_DEG_LAT
lat1 = osm_lib.ORIGIN_LAT + y1 / osm_lib.M_PER_DEG_LAT
lon0 = osm_lib.ORIGIN_LON + x0 / osm_lib.M_PER_DEG_LON
lon1 = osm_lib.ORIGIN_LON + x1 / osm_lib.M_PER_DEG_LON

params = {
    "SERVICE": "WFS", "VERSION": "2.0.0", "REQUEST": "GetFeature",
    "TYPENAMES": "BDTOPO_V3:batiment",
    "BBOX": f"{lat0:.6f},{lon0:.6f},{lat1:.6f},{lon1:.6f},urn:ogc:def:crs:EPSG::4326",
    "OUTPUTFORMAT": "application/json", "COUNT": "10000",
}
url = "https://data.geopf.fr/wfs/ows?" + urllib.parse.urlencode(params)
print("GET", url)
with urllib.request.urlopen(url, timeout=180) as r:
    data = json.load(r)
feats = data.get("features", [])
if not feats:
    raise SystemExit("no buildings returned")
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(data), encoding="utf-8")
with_h = sum(1 for f in feats if f["properties"].get("hauteur") is not None)
print(f"wrote {OUT}: {len(feats)} buildings, {with_h} with a measured height")
