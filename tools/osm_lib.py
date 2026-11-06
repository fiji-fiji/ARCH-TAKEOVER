"""Shared OpenStreetMap helpers: parse data/raw/overpass.json into local-metre geometry.

Coordinate frame ("map metres"):
    origin = 347 Rue Saint-Martin, x = metres east, y = metres north.
Unreal later maps this to  X = x*100 (cm), Y = -y*100 (UE is left-handed, +Y points "south"), Z up.
"""
import json, math, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"

ORIGIN_LAT, ORIGIN_LON = 48.8682801, 2.3550525
M_PER_DEG_LAT = 111_132.0
M_PER_DEG_LON = 111_320.0 * math.cos(math.radians(ORIGIN_LAT))   # ~73 230 m


def project(lat, lon):
    return ((lon - ORIGIN_LON) * M_PER_DEG_LON, (lat - ORIGIN_LAT) * M_PER_DEG_LAT)


ROAD_WIDTH = {   # carriageway width in metres, by OSM highway class (Paris inner-city typical)
    "primary": 14.0, "secondary": 11.0, "tertiary": 9.0, "residential": 7.0, "unclassified": 7.0,
    "living_street": 6.0, "pedestrian": 6.0, "service": 4.0, "footway": 2.5, "path": 2.0,
    "steps": 2.0, "cycleway": 2.5,
}


def parse_height(tags):
    h = tags.get("height")
    if h:
        try:
            return float(str(h).replace("m", "").strip())
        except ValueError:
            pass
    lv = tags.get("building:levels")
    if lv:
        try:
            return 3.0 * float(lv) + 1.5          # 3 m per floor + roof parapet
        except ValueError:
            pass
    return 19.0                                   # Haussmann default (~6 floors)


def load():
    data = json.loads((RAW / "overpass.json").read_text(encoding="utf-8"))
    nodes = {e["id"]: e for e in data["elements"] if e["type"] == "node"}
    ways = {e["id"]: e for e in data["elements"] if e["type"] == "way"}
    rels = [e for e in data["elements"] if e["type"] == "relation"]

    def way_pts(w):
        return [project(nodes[n]["lat"], nodes[n]["lon"]) for n in w["nodes"] if n in nodes]

    buildings, roads, greens, trees, pois, barriers = [], [], [], [], [], []
    used_in_rel = set()

    for r in rels:
        tags = r.get("tags", {})
        if "building" not in tags and not tags.get("leisure"):
            continue
        outers = []
        for m in r.get("members", []):
            if m["type"] == "way" and m["ref"] in ways:
                used_in_rel.add(m["ref"])
                if m.get("role", "outer") == "outer":
                    outers.append(way_pts(ways[m["ref"]]))
        for ring in stitch(outers):
            if len(ring) >= 3:
                rec = {"pts": ring, "tags": tags, "id": f"r{r['id']}"}
                if "building" in tags:
                    rec["height"] = parse_height(tags)
                    buildings.append(rec)
                else:
                    greens.append(rec)

    for w in ways.values():
        tags = w.get("tags", {})
        pts = way_pts(w)
        if len(pts) < 2:
            continue
        if "building" in tags and w["id"] not in used_in_rel:
            if pts[0] == pts[-1]:
                pts = pts[:-1]
            if len(pts) >= 3:
                buildings.append({"pts": pts, "tags": tags, "id": f"w{w['id']}", "height": parse_height(tags)})
        elif "highway" in tags:
            roads.append({"pts": pts, "tags": tags, "id": f"w{w['id']}",
                          "width": ROAD_WIDTH.get(tags["highway"], 5.0), "name": tags.get("name", "")})
        elif (tags.get("leisure") in ("park", "garden", "playground", "pitch") or
              tags.get("landuse") in ("grass", "forest", "village_green")) and w["id"] not in used_in_rel:
            if pts[0] == pts[-1]:
                pts = pts[:-1]
            greens.append({"pts": pts, "tags": tags, "id": f"w{w['id']}"})
        elif "barrier" in tags:
            barriers.append({"pts": pts, "tags": tags, "id": f"w{w['id']}"})

    for n in nodes.values():
        tags = n.get("tags")
        if not tags:
            continue
        p = project(n["lat"], n["lon"])
        if tags.get("natural") == "tree":
            trees.append(p)
        elif any(k in tags for k in ("amenity", "shop", "historic", "tourism", "railway")):
            pois.append({"pt": p, "tags": tags, "name": tags.get("name", "")})

    apply_ign_heights(buildings)
    return {"buildings": buildings, "roads": roads, "greens": greens, "trees": trees,
            "pois": pois, "barriers": barriers}


def apply_ign_heights(buildings):
    """Replace estimated heights with IGN BD TOPO measured eaves heights (data/raw/ign_batiment.json).

    Match: the IGN building whose centroid falls inside the OSM footprint (largest one wins), else the
    IGN footprint that contains the OSM centroid. An explicit OSM `height` tag always wins.
    """
    path = RAW / "ign_batiment.json"
    if not path.exists():
        return 0
    feats = json.loads(path.read_text(encoding="utf-8")).get("features", [])
    ign = []
    for f in feats:
        h = f["properties"].get("hauteur")
        if h is None or h <= 0:
            continue
        geom = f["geometry"]
        polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]
        for poly in polys:
            ring = [project(c[1], c[0]) for c in poly[0]]
            if len(ring) >= 3:
                ign.append({"ring": ring, "c": centroid(ring), "bbox": bbox_of(ring), "area": abs(poly_area(ring)),
                            "h": float(h), "floors": f["properties"].get("nombre_d_etages")})
    matched = 0
    for b in buildings:
        if b["tags"].get("height"):
            continue
        ring = b["pts"]; bb = bbox_of(ring)
        best = None
        for g in ign:
            if g["c"][0] < bb[0] or g["c"][0] > bb[2] or g["c"][1] < bb[1] or g["c"][1] > bb[3]:
                continue
            if point_in_poly(g["c"], ring) and (best is None or g["area"] > best["area"]):
                best = g
        if best is None:
            # no IGN centroid inside (L-shaped / split footprints): the IGN polygon that contains the
            # most of the OSM outline (vertices pulled 0.6 m inwards + centroid) wins
            c = centroid(ring)
            samples = [c]
            for p in ring:
                d = (c[0] - p[0], c[1] - p[1]); L = math.hypot(*d) or 1.0
                samples.append((p[0] + d[0] / L * 0.6, p[1] + d[1] / L * 0.6))
            best_n = 1
            for g in ign:
                gb = g["bbox"]
                if gb[2] < bb[0] or gb[0] > bb[2] or gb[3] < bb[1] or gb[1] > bb[3]:
                    continue
                cnt = sum(1 for s in samples if point_in_poly(s, g["ring"]))
                if cnt > best_n:
                    best, best_n = g, cnt
        if best is not None:
            b["height"] = max(3.0, best["h"])
            b["ign_floors"] = best["floors"]
            matched += 1
    return matched


def clean_ring(pts, eps=0.05):
    """Drop duplicate points (closer than eps), force counter-clockwise order (outward normal of p->q is (dy, -dx))."""
    out = []
    for p in pts:
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > eps:
            out.append(p)
    if len(out) > 1 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) < eps:
        out.pop()
    if len(out) >= 3 and poly_area(out) < 0:
        out.reverse()
    return out


def ring_touches(ring, x0, y0, x1, y1):
    bx0, by0, bx1, by1 = bbox_of(ring)
    return not (bx1 < x0 or bx0 > x1 or by1 < y0 or by0 > y1)


def stitch(segments):
    """Join open way segments sharing endpoints into closed rings."""
    segs = [list(s) for s in segments if len(s) >= 2]
    rings = []
    while segs:
        ring = segs.pop(0)
        changed = True
        while changed and ring[0] != ring[-1]:
            changed = False
            for i, s in enumerate(segs):
                if s[0] == ring[-1]:
                    ring += s[1:]; segs.pop(i); changed = True; break
                if s[-1] == ring[-1]:
                    ring += list(reversed(s))[1:]; segs.pop(i); changed = True; break
                if s[-1] == ring[0]:
                    ring = s[:-1] + ring; segs.pop(i); changed = True; break
                if s[0] == ring[0]:
                    ring = list(reversed(s))[:-1] + ring; segs.pop(i); changed = True; break
        if ring[0] == ring[-1]:
            ring = ring[:-1]
        rings.append(ring)
    return rings


# ---- small geometry helpers -------------------------------------------------
def poly_area(pts):
    a = 0.0
    for i in range(len(pts)):
        x1, y1 = pts[i]; x2, y2 = pts[(i + 1) % len(pts)]
        a += x1 * y2 - x2 * y1
    return a / 2.0


def centroid(pts):
    a = poly_area(pts)
    if abs(a) < 1e-9:
        return (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))
    cx = cy = 0.0
    for i in range(len(pts)):
        x1, y1 = pts[i]; x2, y2 = pts[(i + 1) % len(pts)]
        f = x1 * y2 - x2 * y1
        cx += (x1 + x2) * f; cy += (y1 + y2) * f
    return (cx / (6 * a), cy / (6 * a))


def point_in_poly(pt, pts):
    x, y = pt; inside = False
    n = len(pts)
    for i in range(n):
        x1, y1 = pts[i]; x2, y2 = pts[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            xi = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if xi > x:
                inside = not inside
    return inside


def bbox_of(pts):
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)


def dist_point_seg(p, a, b):
    px, py = p; ax, ay = a; bx, by = b
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))
