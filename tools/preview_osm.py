"""Render data/raw/overpass.json to data/preview_osm.png (top-down) and list named streets / POIs.
Used to design the playable zone by eye.
Usage: python -I tools/preview_osm.py [data/map/layout.json]   (optional overlay of zone / sites / spawns)
"""
import sys, json, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib
from pnglib import Canvas

d = osm_lib.load()
SCALE = 2.0   # px per metre
W = H = 1400
cx, cy = W / 2, H / 2


def P(p):
    return (cx + p[0] * SCALE, cy - p[1] * SCALE)


c = Canvas(W, H, (235, 230, 220))
for g in d["greens"]:
    c.polygon([P(p) for p in g["pts"]], (160, 205, 150))
for r in d["roads"]:
    c.polyline([P(p) for p in r["pts"]], (255, 255, 255), r["width"] * SCALE)
for b in d["buildings"]:
    shade = max(90, 200 - int(b["height"] * 4))
    c.polygon([P(p) for p in b["pts"]], (shade, shade - 10, shade - 20))
for t in d["trees"]:
    x, y = P(t); c.circle(x, y, 2, (40, 120, 40))
for m in range(-350, 351, 50):          # 50 m grid
    x, y = P((m, m))
    c.line((x, 0), (x, H), (200, 200, 230), 1); c.line((0, y), (W, y), (200, 200, 230), 1)
    c.text(int(x) + 2, 2, str(m), (60, 60, 120)); c.text(2, int(y) + 2, str(m), (60, 60, 120))
seen = set()
for r in d["roads"]:
    if r["name"] and r["name"] not in seen and len(r["pts"]) >= 2:
        seen.add(r["name"]); x, y = P(r["pts"][len(r["pts"]) // 2])
        c.text(int(x), int(y), r["name"].upper()[:26], (180, 0, 0))
x, y = P((0, 0)); c.circle(x, y, 6, (255, 0, 0)); c.text(int(x) + 8, int(y) - 8, "HOME 347", (255, 0, 0), 2)

if len(sys.argv) > 1:
    z = json.loads(pathlib.Path(sys.argv[1]).read_text())
    if "zone" in z:
        pts = [P(p) for p in z["zone"]]
        for a, b in zip(pts, pts[1:] + pts[:1]):
            c.line(a, b, (0, 0, 255), 3)
    for name, key, col in [("A", "siteA", (255, 80, 0)), ("B", "siteB", (255, 80, 0)),
                           ("ATK", "spawnT", (200, 160, 0)), ("DEF", "spawnCT", (0, 120, 255))]:
        if key in z:
            x, y = P(z[key]["center"]); c.circle(x, y, 10, col); c.text(int(x) - 8, int(y) - 4, name, (255, 255, 255), 2)

out = osm_lib.ROOT / "data" / "preview_osm.png"
c.save(out); print("wrote", out)
print("buildings", len(d["buildings"]), "roads", len(d["roads"]), "greens", len(d["greens"]),
      "trees", len(d["trees"]), "pois", len(d["pois"]))
names = {}
for r in d["roads"]:
    if r["name"]:
        xs = [p[0] for p in r["pts"]]; ys = [p[1] for p in r["pts"]]
        n = names.setdefault(r["name"], [r["tags"]["highway"], 1e9, 1e9, -1e9, -1e9])
        n[1] = min(n[1], min(xs)); n[2] = min(n[2], min(ys)); n[3] = max(n[3], max(xs)); n[4] = max(n[4], max(ys))
for k, v in sorted(names.items()):
    print(f"  {k:40s} {v[0]:14s} x[{v[1]:6.0f},{v[3]:6.0f}] y[{v[2]:6.0f},{v[4]:6.0f}]")
for g in d["greens"]:
    print("  GREEN", g["tags"].get("name", "?"), [round(v) for v in osm_lib.centroid(g["pts"])])
for p in d["pois"]:
    if p["name"] and abs(p["pt"][0]) < 200 and abs(p["pt"][1]) < 200:
        t = p["tags"]
        print("  POI", p["name"], t.get("amenity") or t.get("shop") or t.get("historic") or t.get("tourism") or t.get("railway"),
              [round(v) for v in p["pt"]])
