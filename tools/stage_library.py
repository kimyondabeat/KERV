#!/usr/bin/env python3
"""Stage a drum pack (one sub-folder per category) into Kit/ as FLAC: <cat>__NN__<name>.flac"""
import re, sys
from pathlib import Path
import numpy as np, soundfile as sf

CATS = {"conga": "conga", "cowbell": "cowbell", "crash": "crash", "cymbal": "cymbal", "fx": "fx",
        "hihat": "hihat", "kick": "kick", "percs": "percs", "rim & clap": "rimclap", "snare": "snare", "tom": "tom"}
src, kit = Path(sys.argv[1]), Path(sys.argv[2])
strip = sys.argv[3] if len(sys.argv) > 3 else ""
kit.mkdir(parents=True, exist_ok=True)
for old in kit.glob("*__*"): old.unlink()
count, total = {}, 0
for f in sorted(src.rglob("*.wav"), key=lambda p: str(p).lower()):
    cat = CATS.get(f.relative_to(src).parts[0].lower())
    if not cat: print("skip", f); continue
    data, sr = sf.read(str(f), always_2d=True, dtype="float32")
    data = data[:, :2][: int(sr * 12)]
    loud = np.where(np.abs(data).max(axis=1) > 10 ** (-70 / 20))[0]
    if len(loud) == 0: continue
    data = data[: loud[-1] + 1]
    if data.shape[1] == 2 and np.allclose(data[:, 0], data[:, 1], atol=1e-4): data = data[:, :1]
    peak = float(np.abs(data).max())
    if peak > 0.999: data *= 0.999 / peak
    name = re.sub(r"[^A-Za-z0-9 ._()#+-]", "_", f.stem.replace(strip, "").strip())[:60].strip()
    count[cat] = count.get(cat, 0) + 1
    out = kit / f"{cat}__{count[cat]:02d}__{name}.flac"
    sf.write(str(out), data, sr, format="FLAC", subtype="PCM_24"); total += out.stat().st_size
print(count, f"{total/1e6:.1f} MB")
