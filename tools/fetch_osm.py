"""Download raw OpenStreetMap data for the neighbourhood around the home address.

Usage:  python -I tools/fetch_osm.py
Writes: data/raw/overpass.json   (raw Overpass API response, untouched)

The bounding box is centred on 347 Rue Saint-Martin (geocoded with Nominatim,
see data/raw/geocode.json) and is deliberately small: a competitive 5v5 bomb map
is ~250 m across, so we only need a few blocks plus a ring of backdrop buildings.
"""
import json, sys, urllib.request, urllib.parse, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
RAW.mkdir(parents=True, exist_ok=True)

CENTER_LAT, CENTER_LON = 48.8682801, 2.3550525   # 347 Rue Saint-Martin
HALF_LAT = 0.0030    # ~335 m north/south
HALF_LON = 0.0046    # ~335 m east/west (at 48.87°N, 1° lon ≈ 73.3 km)

bbox = f"{CENTER_LAT-HALF_LAT},{CENTER_LON-HALF_LON},{CENTER_LAT+HALF_LAT},{CENTER_LON+HALF_LON}"
query = f"""
[out:json][timeout:90][bbox:{bbox}];
(
  way["building"];
  relation["building"];
  way["highway"];
  way["leisure"~"park|garden|playground|pitch"];
  way["landuse"~"grass|forest|village_green"];
  relation["leisure"~"park|garden"];
  way["amenity"~"fountain|parking"];
  node["natural"="tree"];
  node["amenity"];
  node["shop"];
  node["historic"];
  node["tourism"];
  node["railway"~"station|subway_entrance"];
  way["barrier"];
);
out body;
>;
out skel qt;
"""
req = urllib.request.Request(
    "https://overpass-api.de/api/interpreter",
    data=urllib.parse.urlencode({"data": query}).encode(),
    headers={"User-Agent": "paris-neighbourhood-fps-map/1.0 (personal project)"},
)
print("Querying Overpass for bbox", bbox)
with urllib.request.urlopen(req, timeout=120) as r:
    payload = r.read()
out = RAW / "overpass.json"
out.write_bytes(payload)
data = json.loads(payload)
kinds = {}
for el in data["elements"]:
    kinds[el["type"]] = kinds.get(el["type"], 0) + 1
print("Saved", out, len(payload), "bytes;", kinds)
(RAW / "bbox.json").write_text(json.dumps({"center": [CENTER_LAT, CENTER_LON], "bbox": bbox}, indent=2))
