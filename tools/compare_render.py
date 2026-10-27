"""Compare band tilts of two rendered vocals against the reference target."""

import sys
from pathlib import Path

import numpy as np
import soundfile as sf

BANDS = 32
CENTERS = 40.0 * (400.0 ** (np.arange(BANDS) / (BANDS - 1)))
EQ_FREQS = np.array([63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 16000], dtype=float)


def band_tilt(path: Path) -> np.ndarray:
    x, sr = sf.read(path, always_2d=True)
    x = x.mean(axis=1).astype(np.float64)
    n = 2048
    hop = 1024
    window = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    positions = np.log(np.maximum(freqs, 1e-9) / 40.0) / np.log(400.0) * (BANDS - 1)
    index = np.rint(positions).astype(int)
    valid = (index >= 0) & (index < BANDS)

    acc = np.zeros(BANDS)
    count = 0

    for start in range(0, len(x) - n, hop):
        frame = x[start:start + n]
        if 20.0 * np.log10(np.sqrt(np.mean(frame ** 2)) + 1e-12) < -45.0:
            continue
        mag = np.abs(np.fft.rfft(frame * window)) ** 2
        per_band = np.zeros(BANDS)
        np.add.at(per_band, index[valid], mag[valid])
        acc += per_band
        count += 1

    db = 10.0 * np.log10(acc / max(count, 1) + 1e-12)
    return db - np.mean(db[(CENTERS >= 200.0) & (CENTERS <= 8000.0)])


def at(values: np.ndarray, freqs: np.ndarray) -> np.ndarray:
    pos = np.log(freqs / 40.0) / np.log(400.0) * (BANDS - 1)
    lo = np.floor(pos).astype(int)
    hi = np.minimum(BANDS - 1, lo + 1)
    t = pos - lo
    return values[lo] * (1 - t) + values[hi] * t


def main() -> None:
    if len(sys.argv) != 5:
        print("usage: compare_render.py <dry_vocal> <reference> <out_fit> <out_ref>")
        raise SystemExit(1)

    dry, ref, fit, matched = (band_tilt(Path(p)) for p in sys.argv[1:])

    target = at(ref - dry, EQ_FREQS)
    actual = at(matched - fit, EQ_FREQS)
    weight = ((EQ_FREQS >= 250) & (EQ_FREQS <= 10000)).astype(float)

    corr = np.corrcoef(target * weight, actual * weight)[0, 1]

    print(f"{'Hz':>7} {'target':>8} {'actual':>8} {'diff':>8}")
    for hz, t, a in zip(EQ_FREQS, target, actual):
        print(f"{hz:7.0f} {t:8.2f} {a:8.2f} {a - t:8.2f}")

    print(f"\ncorrelation target vs actual: {corr:.3f}")
    print(f"rms target {np.sqrt(np.mean(target ** 2)):.2f} dB, "
          f"rms actual {np.sqrt(np.mean(actual ** 2)):.2f} dB")


if __name__ == "__main__":
    main()
