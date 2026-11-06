"""Turn the Quaternius "Ultimate Gun Pack" (CC0) into the game's weapon meshes under assets/models/quaternius_guns/.

Usage: python -I tools/import_gun_pack.py [--src <zip or extracted folder>] [--blender "<blender.exe>"]

Default source: ultimate_gun_pack_by_quaternius.zip (or the extracted "Ultimate Gun Pack*" folder) in Downloads.
Get it from https://quaternius.com/packs/ultimategun.html or https://opengameart.org/content/low-poly-guns-pack.
The pack is unzipped into assets/models/quaternius_guns/_src/ (not committed); tools/blender/export_guns.py then
runs inside Blender and writes gun_<key>.fbx + pieces.json, which build_level.py imports as /Game/Models/quaternius_guns/.
"""
import argparse, pathlib, subprocess, sys, shutil, os, zipfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

ROOT = osm_lib.ROOT
DOWNLOADS = pathlib.Path.home() / "Downloads"
DEFAULT_BLENDER = [r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe", r"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe"]

ap = argparse.ArgumentParser()
ap.add_argument("--src", default=None)
ap.add_argument("--blender", default=None)
args = ap.parse_args()

src = pathlib.Path(args.src) if args.src else None
if src is None:
    for cand in sorted(DOWNLOADS.glob("*quaternius*.zip")) + sorted(DOWNLOADS.glob("Ultimate Gun Pack*")):
        src = cand
        break
if src is None or not src.exists():
    sys.exit("gun pack not found: pass --src <zip or folder> (download it from https://quaternius.com/packs/ultimategun.html)")

out = ROOT / "assets" / "models" / "quaternius_guns"
work = out / "_src"
out.mkdir(parents=True, exist_ok=True)
if src.is_file():
    if work.exists():
        shutil.rmtree(work)
    with zipfile.ZipFile(src) as z:
        z.extractall(work)
    root = work
else:
    root = src
fbx_dir = next((p for p in root.rglob("FBX") if p.is_dir()), None)
if fbx_dir is None:
    sys.exit(f"no FBX folder under {root}")

blender = args.blender or shutil.which("blender") or next((p for p in DEFAULT_BLENDER if os.path.exists(p)), None)
if not blender:
    sys.exit("Blender not found (pass --blender)")
script = ROOT / "tools" / "blender" / "export_guns.py"
cmd = [blender, "-b", "-P", str(script), "--", str(fbx_dir), str(out)]
print("==", " ".join(f'"{c}"' if " " in c else c for c in cmd))
r = subprocess.run(cmd, cwd=str(out), text=True, capture_output=True, encoding="utf-8", errors="replace")
for line in r.stdout.splitlines():
    if "[export_guns]" in line or "Error" in line or "Traceback" in line:
        print(line)
if r.returncode != 0:
    print(r.stderr[-3000:])
    sys.exit(f"blender exited with {r.returncode}")
(out / "LICENSE.md").write_text("Gun meshes: Quaternius, Ultimate Gun Pack (https://quaternius.com), CC0 1.0 Universal.\n"
                                "Normalised and re-exported by tools/blender/export_guns.py.\n", encoding="utf-8")
print("wrote", out)
