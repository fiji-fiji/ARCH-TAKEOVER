"""Generate all PNG textures procedurally into assets/textures/ (no Pillow: uses tools/pnglib.py).

Tiles are designed in metres so the mesh exporter can set UVs = metres / tile_size:
    facade_haussmann.png  3 m x 3 m   one window bay x one floor
    shopfront.png         4 m x 4 m   ground-floor shop band
    roof_zinc.png         3 m x 3 m
    asphalt.png           4 m x 4 m   plain carriageway / ground
    road.png              4 m x 4 m   carriageway strip with white dashed centre line (u along road, v across)
    pavement.png          2 m x 2 m   Paris bitumen sidewalk with granite speckle
    grass.png             2 m x 2 m
    tarp.png              2 m x 2 m   scaffolding mesh
Usage: python -I tools/make_textures.py
"""
import sys, pathlib, random, math
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pnglib import Canvas

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "textures"
OUT.mkdir(parents=True, exist_ok=True)
rng = random.Random(1337)


def clamp(v):
    return max(0, min(255, int(v)))


def noise_fill(c, base, amp, seed=0, blotch=None):
    """Per-pixel speckle noise plus optional soft blotches, cheap and tileable (no gradients across edges)."""
    r = random.Random(seed)
    w, h = c.w, c.h
    px = c.px
    for y in range(h):
        row = y * w * 3
        for x in range(w):
            n = r.random() * 2 - 1
            i = row + x * 3
            px[i] = clamp(base[0] + n * amp); px[i + 1] = clamp(base[1] + n * amp); px[i + 2] = clamp(base[2] + n * amp)
    if blotch:
        count, rad, delta = blotch
        for _ in range(count):
            cx, cy = r.randrange(w), r.randrange(h)
            rr = r.randint(rad // 2, rad)
            d = r.uniform(-delta, delta)
            for yy in range(-rr, rr + 1):
                for xx in range(-rr, rr + 1):
                    if xx * xx + yy * yy <= rr * rr:
                        X, Y = (cx + xx) % w, (cy + yy) % h   # wrap → tileable
                        i = (Y * w + X) * 3
                        px[i] = clamp(px[i] + d); px[i + 1] = clamp(px[i + 1] + d); px[i + 2] = clamp(px[i + 2] + d)


def darken_rect(c, x0, y0, x1, y1, f):
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            r, g, b = c.get(x, y)
            c.set(x, y, (clamp(r * f), clamp(g * f), clamp(b * f)))


# ---------------------------------------------------------------- facade (3 m x 3 m, one bay x one floor)
def facade():
    S = 512
    c = Canvas(S, S, (214, 200, 178))
    noise_fill(c, (214, 200, 178), 9, seed=1, blotch=(40, 40, 8))
    px_per_m = S / 3.0
    # stone courses every 0.5 m (horizontal joints) and staggered vertical joints
    for k in range(6):
        y = int(k * 0.5 * px_per_m)
        c.rect(0, y, S, y + 2, (180, 168, 148))
        off = (k % 2) * 0.6
        for j in range(3):
            x = int(((j * 1.2) + off) * px_per_m) % S
            c.rect(x, y, x + 2, y + int(0.5 * px_per_m), (186, 174, 154))
    # floor slab / string course at the bottom of the tile
    c.rect(0, S - int(0.12 * px_per_m), S, S, (170, 158, 138))
    # window: 1.1 m wide x 2.2 m tall, centred, French window with sill at 0.25 m above floor line
    wx0 = int((1.5 - 0.55) * px_per_m); wx1 = int((1.5 + 0.55) * px_per_m)
    wy1 = S - int(0.30 * px_per_m); wy0 = wy1 - int(2.2 * px_per_m)
    # stone surround (slightly lighter) + reveal shadow
    c.rect(wx0 - 14, wy0 - 14, wx1 + 14, wy1 + 8, (200, 188, 166))
    c.rect(wx0 - 4, wy0 - 4, wx1 + 4, wy1 + 4, (90, 84, 78))
    # glass: dark blue-grey with reflection gradient
    for y in range(wy0, wy1):
        t = (y - wy0) / max(1, wy1 - wy0)
        col = (clamp(60 + 40 * (1 - t)), clamp(70 + 45 * (1 - t)), clamp(85 + 50 * (1 - t)))
        c.rect(wx0, y, wx1, y + 1, col)
    # white frame: outer + mullion + transom
    f = (235, 232, 225)
    c.rect(wx0, wy0, wx1, wy0 + 6, f); c.rect(wx0, wy1 - 6, wx1, wy1, f)
    c.rect(wx0, wy0, wx0 + 6, wy1, f); c.rect(wx1 - 6, wy0, wx1, wy1, f)
    mx = (wx0 + wx1) // 2; c.rect(mx - 3, wy0, mx + 3, wy1, f)
    ty = wy0 + int(0.6 * px_per_m); c.rect(wx0, ty - 3, wx1, ty + 3, f)
    # wrought-iron guard rail across the lower 0.9 m of the window
    ry0 = wy1 - int(0.9 * px_per_m)
    iron = (30, 30, 34)
    c.rect(wx0 - 10, ry0, wx1 + 10, ry0 + 4, iron); c.rect(wx0 - 10, wy1 - 2, wx1 + 10, wy1 + 2, iron)
    for k in range(9):
        x = wx0 - 10 + int(k * (wx1 - wx0 + 20) / 8)
        c.rect(x, ry0, x + 3, wy1, iron)
        # little scroll
        c.circle(x + 1, ry0 + int(0.25 * px_per_m), 4, iron)
    # shutters (closed on some tiles would break the repeat; keep open shutters at sides)
    sh = (118, 128, 110)
    c.rect(wx0 - 40, wy0, wx0 - 16, wy1 - int(0.9 * px_per_m), sh)
    c.rect(wx1 + 16, wy0, wx1 + 40, wy1 - int(0.9 * px_per_m), sh)
    c.save(OUT / "facade_haussmann.png")


# ---------------------------------------------------------------- shopfront (4 m x 4 m)
def shopfront():
    S = 512
    c = Canvas(S, S, (52, 60, 54))
    noise_fill(c, (52, 60, 54), 6, seed=2)
    ppm = S / 4.0
    # sign band: top 0.8 m, dark green with cream text-ish bars
    c.rect(0, 0, S, int(0.8 * ppm), (28, 60, 44))
    c.rect(int(0.3 * ppm), int(0.25 * ppm), int(3.7 * ppm), int(0.55 * ppm), (222, 206, 160))
    for k in range(5):
        x = int((0.5 + k * 0.65) * ppm)
        c.rect(x, int(0.3 * ppm), x + int(0.4 * ppm), int(0.5 * ppm), (28, 60, 44))
    # big shop window with lit interior
    wx0, wy0, wx1, wy1 = int(0.3 * ppm), int(1.0 * ppm), int(2.6 * ppm), int(3.6 * ppm)
    c.rect(wx0 - 6, wy0 - 6, wx1 + 6, wy1 + 6, (20, 24, 22))
    for y in range(wy0, wy1):
        t = (y - wy0) / (wy1 - wy0)
        c.rect(wx0, y, wx1, y + 1, (clamp(120 + 60 * t), clamp(100 + 50 * t), clamp(70 + 40 * t)))
    # shelves silhouettes
    for k in range(3):
        y = wy0 + int((0.6 + k * 0.8) * ppm)
        c.rect(wx0 + 10, y, wx1 - 10, y + 8, (80, 60, 40))
        for j in range(6):
            x = wx0 + 20 + int(j * (wx1 - wx0 - 40) / 6)
            c.rect(x, y - 30, x + 14, y, (rng.randint(90, 200), rng.randint(60, 160), rng.randint(40, 120)))
    # door at the right: 1.0 m wide
    dx0, dx1 = int(2.8 * ppm), int(3.8 * ppm)
    c.rect(dx0 - 6, wy0 - 6, dx1 + 6, S, (20, 24, 22))
    c.rect(dx0, wy0, dx1, S, (70, 90, 80))
    c.rect(dx0 + 10, wy0 + 10, dx1 - 10, int(2.8 * ppm), (90, 110, 120))
    c.circle(dx1 - 20, int(2.5 * ppm), 5, (220, 200, 90))
    # base plinth 0.3 m stone
    c.rect(0, S - int(0.3 * ppm), S, S, (110, 108, 100))
    c.save(OUT / "shopfront.png")


# ---------------------------------------------------------------- zinc roof (3 m x 3 m)
def roof():
    S = 256
    c = Canvas(S, S, (128, 132, 138))
    noise_fill(c, (128, 132, 138), 10, seed=3, blotch=(30, 20, 10))
    ppm = S / 3.0
    for k in range(5):
        x = int(k * 0.6 * ppm)
        c.rect(x, 0, x + 3, S, (98, 102, 108)); c.rect(x + 3, 0, x + 5, S, (150, 154, 160))
    c.save(OUT / "roof_zinc.png")


# ---------------------------------------------------------------- asphalt / road (4 m x 4 m)
def asphalt():
    S = 256
    c = Canvas(S, S, (62, 62, 64))
    noise_fill(c, (62, 62, 64), 14, seed=4, blotch=(60, 24, 8))
    c.save(OUT / "asphalt.png")
    # road strip: u along road (x), v across (y). Centre dashed line at v=0.5, edge lines at 0.06 / 0.94
    r = Canvas(S, S, (58, 58, 60))
    noise_fill(r, (58, 58, 60), 14, seed=5, blotch=(50, 24, 8))
    mid = S // 2
    r.rect(0, mid - 3, int(S * 0.5), mid + 3, (225, 225, 215))        # dash: 2 m on, 2 m off
    r.rect(0, int(S * 0.05), S, int(S * 0.05) + 4, (210, 210, 200))
    r.rect(0, int(S * 0.95) - 4, S, int(S * 0.95), (210, 210, 200))
    r.save(OUT / "road.png")


# ---------------------------------------------------------------- pavement (2 m x 2 m)
def pavement():
    S = 256
    c = Canvas(S, S, (132, 128, 122))
    noise_fill(c, (132, 128, 122), 12, seed=6, blotch=(80, 16, 10))
    # granite speckle
    r = random.Random(7)
    for _ in range(1800):
        x, y = r.randrange(S), r.randrange(S)
        v = r.randint(90, 190)
        c.set(x, y, (v, v, v - 4))
    c.save(OUT / "pavement.png")


# ---------------------------------------------------------------- grass (2 m x 2 m)
def grass():
    S = 256
    c = Canvas(S, S, (84, 122, 52))
    noise_fill(c, (84, 122, 52), 16, seed=8, blotch=(70, 20, 12))
    r = random.Random(9)
    for _ in range(3000):
        x, y = r.randrange(S), r.randrange(S)
        c.rect(x, y, x + 1, y + r.randint(2, 5), (clamp(70 + r.randint(0, 40)), clamp(110 + r.randint(0, 50)), 40))
    c.save(OUT / "grass.png")


# ---------------------------------------------------------------- scaffolding tarp (2 m x 2 m)
def tarp():
    S = 128
    c = Canvas(S, S, (228, 230, 226))
    noise_fill(c, (228, 230, 226), 8, seed=10)
    for k in range(0, S, 8):
        c.rect(k, 0, k + 1, S, (200, 202, 198)); c.rect(0, k, S, k + 1, (200, 202, 198))
    c.save(OUT / "tarp.png")


if __name__ == "__main__":
    for fn in (facade, shopfront, roof, asphalt, pavement, grass, tarp):
        fn(); print("wrote", fn.__name__)
    print("textures in", OUT)
