"""Synthesise the game's sound effects (no samples, numpy only) -> assets/sounds/*.wav (44.1 kHz mono 16-bit).

Usage: python -I tools/make_sounds.py
"""
import sys, pathlib, wave, struct
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import osm_lib

OUT = osm_lib.ROOT / "assets" / "sounds"
OUT.mkdir(parents=True, exist_ok=True)
SR = 44100
rng = np.random.default_rng(7)


def t(sec):
    return np.arange(int(SR * sec)) / SR


def env(n, a=0.002, d=0.1, s=0.0, r=0.05, hold=0.0):
    """ADSR-ish envelope over n samples."""
    ia, ih, id_, ir = int(a * SR), int(hold * SR), int(d * SR), int(r * SR)
    segs = [np.linspace(0, 1, ia), np.full(ih, 1.0), np.linspace(1, s, id_), np.linspace(s, 0, ir)]
    e = np.concatenate(segs) if sum(len(x) for x in segs) else np.zeros(1)
    return e[:n] if len(e) >= n else np.pad(e, (0, n - len(e)))


def noise(sec):
    return rng.standard_normal(int(SR * sec))


def lowpass(x, cutoff):
    """One-pole low-pass (cutoff Hz), vectorised via cumulative recursion in chunks."""
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.zeros_like(x); acc = 0.0
    for i in range(len(x)):                       # fine for < 1 s clips
        acc = a * acc + (1 - a) * x[i]; y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def sine(freq, sec, phase=0.0):
    return np.sin(2 * np.pi * freq * t(sec) + phase)


def sweep(f0, f1, sec):
    tt = t(sec); f = f0 * (f1 / f0) ** (tt / sec)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def sat(x, k=2.0):
    return np.tanh(x * k) / np.tanh(k)


def save(name, x, gain=0.9):
    x = np.asarray(x, dtype=np.float64)
    x = x / (np.max(np.abs(x)) + 1e-9) * gain
    with wave.open(str(OUT / f"{name}.wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())
    print(f"  {name}.wav  {len(x) / SR:.2f}s")


# ---- rifle shot: sharp transient + low thump + tail
n = int(SR * 0.45)
crack = highpass(noise(0.45), 1500) * env(n, 0.0005, 0.03, 0.15, 0.25)
thump = sweep(180, 50, 0.45) * env(n, 0.001, 0.08, 0.1, 0.2)
tail = lowpass(noise(0.45), 900) * env(n, 0.01, 0.2, 0.1, 0.2) * 0.5
save("S_RifleShot", sat(crack * 1.2 + thump * 1.6 + tail, 3.0))

# ---- dry fire: metallic click
n = int(SR * 0.08)
save("S_DryFire", highpass(noise(0.08), 3000) * env(n, 0.0005, 0.02, 0.0, 0.03) + sine(2400, 0.08) * env(n, 0.0005, 0.01, 0, 0.02) * 0.4)

# ---- reload: mag out (clack), slide, mag in (clack), bolt
parts = []
def click(freq, sec, bright=True):
    n = int(SR * sec)
    x = (highpass(noise(sec), 2500) if bright else lowpass(noise(sec), 1200)) * env(n, 0.0005, sec * 0.4, 0.0, sec * 0.3)
    return x + sine(freq, sec) * env(n, 0.0005, sec * 0.2, 0, sec * 0.2) * 0.5
gap = lambda s: np.zeros(int(SR * s))
parts = [click(900, 0.12, False), gap(0.25), lowpass(noise(0.3), 600) * env(int(SR * 0.3), 0.05, 0.2, 0.2, 0.05) * 0.4,
         gap(0.35), click(1400, 0.1), gap(0.5), click(600, 0.15, False), gap(0.1), click(2000, 0.08)]
save("S_Reload", np.concatenate(parts))

# ---- footsteps (3 variants, pavement)
for k in range(3):
    n = int(SR * 0.14)
    x = lowpass(noise(0.14), 500 + 150 * k) * env(n, 0.002, 0.05, 0.1, 0.08) + highpass(noise(0.14), 2500) * env(n, 0.001, 0.015, 0, 0.02) * 0.4
    save(f"S_Step{k}", x, 0.6)

# ---- hit marker / headshot / death
n = int(SR * 0.09)
save("S_Hit", (sine(1800, 0.09) + sine(2700, 0.09) * 0.5) * env(n, 0.001, 0.05, 0, 0.03))
n = int(SR * 0.16)
save("S_Headshot", (sine(2400, 0.16) + sine(3600, 0.16) * 0.6 + highpass(noise(0.16), 4000) * 0.3) * env(n, 0.001, 0.08, 0.1, 0.07))
n = int(SR * 0.6)
save("S_Death", (sweep(300, 60, 0.6) + lowpass(noise(0.6), 400) * 0.8) * env(n, 0.005, 0.3, 0.2, 0.25))

# ---- interaction tick (plant / defuse progress) and bomb beep
n = int(SR * 0.05)
save("S_Tick", sine(1200, 0.05) * env(n, 0.001, 0.02, 0, 0.02), 0.5)
n = int(SR * 0.12)
save("S_BombBeep", (sine(2200, 0.12) + sine(4400, 0.12) * 0.2) * env(n, 0.001, 0.06, 0.3, 0.05), 0.7)
n = int(SR * 0.5)
save("S_BombPlanted", np.concatenate([sine(880, 0.12) * env(int(SR * 0.12), 0.002, 0.08, 0.2, 0.02), gap(0.05),
                                       sine(660, 0.3) * env(int(SR * 0.3), 0.002, 0.2, 0.2, 0.08)]))
save("S_BombDefused", np.concatenate([sine(660, 0.1) * env(int(SR * 0.1), 0.002, 0.06, 0.3, 0.02), gap(0.03),
                                       sine(880, 0.1) * env(int(SR * 0.1), 0.002, 0.06, 0.3, 0.02), gap(0.03),
                                       sine(1320, 0.35) * env(int(SR * 0.35), 0.002, 0.25, 0.2, 0.08)]))

# ---- explosion: sub boom + crackle + long rumble
n = int(SR * 2.5)
boom = sweep(90, 25, 2.5) * env(n, 0.002, 0.6, 0.2, 1.5)
crackle = highpass(noise(2.5), 800) * env(n, 0.001, 0.15, 0.1, 0.6)
rumble = lowpass(noise(2.5), 180) * env(n, 0.02, 1.0, 0.3, 1.2)
save("S_Explosion", sat(boom * 1.5 + crackle * 0.8 + rumble * 1.2, 2.5))

# ---- round jingles
def chord(freqs, sec, a=0.01, r=0.3):
    n = int(SR * sec)
    return sum(sine(f, sec) * (0.8 ** i) for i, f in enumerate(freqs)) * env(n, a, sec * 0.3, 0.4, r)
save("S_RoundStart", np.concatenate([chord([523, 659], 0.15), chord([659, 784], 0.15), chord([784, 1047, 1319], 0.5, r=0.4)]))
save("S_RoundWin", np.concatenate([chord([523, 659, 784], 0.2), chord([587, 740, 880], 0.2), chord([659, 831, 988, 1319], 0.8, r=0.6)]))
save("S_RoundLose", np.concatenate([chord([440, 523], 0.3), chord([415, 494], 0.3), chord([392, 466, 233], 0.9, r=0.6)]))
save("S_MatchEnd", np.concatenate([chord([392, 494, 587], 0.25), chord([440, 554, 659], 0.25), chord([494, 622, 740], 0.25), chord([523, 659, 784, 1047], 1.2, r=0.8)]))
print("done")
