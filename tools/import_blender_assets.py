"""Turn user-supplied Blender packs into game pieces (FBX + textures) under assets/models/<pack>/.

Usage: python -I tools/import_blender_assets.py [--src "<folder with the .blend and Textures.zip>"] [--blender "<blender.exe>"]

Pack 1: "3 Piece Parisian Apartment Building" (Downloads / Parisian Building Blender Assets) -> assets/models/parisian_block/
        via tools/blender/export_parisian.py (runs inside Blender headless).
"""
import argparse, pathlib, subprocess, sys, shutil, os
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

ROOT = osm_lib.ROOT
DEFAULT_SRC = pathlib.Path.home() / "Downloads" / "Parisian Building Blender Assets"
DEFAULT_BLENDER = [r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe", r"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe"]

ap = argparse.ArgumentParser()
ap.add_argument("--src", default=str(DEFAULT_SRC))
ap.add_argument("--blender", default=None)
args = ap.parse_args()

src = pathlib.Path(args.src)
blend = next(iter(src.glob("*.blend")), None)
texzip = src / "Textures.zip"
if not blend or not texzip.exists():
    sys.exit(f"pack not found in {src} (need a .blend and Textures.zip)")
blender = args.blender or shutil.which("blender") or next((p for p in DEFAULT_BLENDER if os.path.exists(p)), None)
if not blender:
    sys.exit("Blender not found (pass --blender)")

out = ROOT / "assets" / "models" / "parisian_block"
out.mkdir(parents=True, exist_ok=True)
script = ROOT / "tools" / "blender" / "export_parisian.py"
cmd = [blender, "-b", str(blend), "-P", str(script), "--", str(texzip), str(out)]
print("==", " ".join(f'"{c}"' if " " in c else c for c in cmd))
r = subprocess.run(cmd, cwd=str(out), text=True, capture_output=True, encoding="utf-8", errors="replace")
for line in r.stdout.splitlines():
    if "[export_parisian]" in line or "Error" in line or "Traceback" in line:
        print(line)
if r.returncode != 0:
    print(r.stderr[-3000:])
    sys.exit(f"blender exited with {r.returncode}")
print("wrote", out)
