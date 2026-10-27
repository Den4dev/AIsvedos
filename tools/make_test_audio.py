"""Generate deterministic synthetic test material for render_cli smoke tests.

Creates vocal-like, beat-like and reference files under test_audio/.
Not musical, but spectrally and dynamically close enough to catch crashes,
NaN, broken gain staging and EQ direction errors.
"""

from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt

SR = 48000
DURATION = 12.0
OUT = Path(__file__).resolve().parent.parent / "test_audio"


def resonator(x: np.ndarray, freq: float, q: float, gain: float) -> np.ndarray:
    sos = butter(2, [max(20.0, freq * (1 - 0.5 / q)), min(SR / 2 - 100, freq * (1 + 0.5 / q))], btype="band", fs=SR, output="sos")
    return gain * sosfilt(sos, x)


def make_vocal(t: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    f0 = 132.0 + 10.0 * np.sin(2 * np.pi * 5.1 * t) + 2.0 * np.sin(2 * np.pi * 0.31 * t)
    phase = 2 * np.pi * np.cumsum(f0) / SR

    dry = np.zeros_like(t)
    for harmonic, amplitude in [(1, 1.0), (2, 0.55), (3, 0.38), (4, 0.22), (5, 0.16), (6, 0.11), (8, 0.06)]:
        dry += amplitude * np.sin(harmonic * phase)

    voiced = (
        resonator(dry, 650, 3.0, 1.0)
        + resonator(dry, 1150, 4.0, 0.7)
        + resonator(dry, 2700, 5.0, 0.45)
        + 0.05 * dry
    )
    breath = sosfilt(butter(2, [4000, 10000], btype="band", fs=SR, output="sos"), rng.normal(0, 1, len(t)))
    voiced = voiced / (np.max(np.abs(voiced)) + 1e-9) + 0.04 * breath

    phrase = np.zeros_like(t)
    for start in np.arange(0.35, DURATION - 1.2, 1.6):
        length = 1.05
        local = (t >= start) & (t < start + length)
        local_t = (t[local] - start) / length
        env = np.sin(np.pi * local_t) ** 0.6
        phrase[local] = env

    return 0.22 * voiced * phrase


def make_beat(t: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    bpm = 140.0
    beat_len = 60.0 / bpm
    out = np.zeros_like(t)

    for k in range(int(DURATION / beat_len) + 1):
        start = k * beat_len
        idx = int(start * SR)
        if idx >= len(t):
            break

        length = int(0.28 * SR)
        end = min(len(t), idx + length)
        local_t = np.arange(end - idx) / SR

        kick = np.sin(2 * np.pi * (110 * np.exp(-local_t * 26) + 45) * local_t) * np.exp(-local_t * 9)
        out[idx:end] += 0.55 * kick

        if k % 2 == 1:
            hat_len = int(0.09 * SR)
            hat_end = min(len(t), idx + hat_len)
            hat_t = np.arange(hat_end - idx) / SR
            hat = rng.normal(0, 1, hat_end - idx) * np.exp(-hat_t * 55)
            hat = sosfilt(butter(2, [6000, 15000], btype="band", fs=SR, output="sos"), hat)
            out[idx:hat_end] += 0.16 * hat

    bass_phase = 2 * np.pi * np.cumsum(55.0 + 0 * t) / SR
    out += 0.18 * np.sin(bass_phase) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.5 * t))
    out += 0.05 * rng.normal(0, 1, len(t)) * 0.0
    return out


def main() -> None:
    rng = np.random.default_rng(7)
    t = np.arange(int(SR * DURATION)) / SR

    vocal = make_vocal(t, rng)
    beat = make_beat(t, rng)

    vocal /= max(1e-9, np.max(np.abs(vocal))) / 0.5
    beat /= max(1e-9, np.max(np.abs(beat))) / 0.85

    reference = 0.75 * beat + 0.65 * vocal
    reference = sosfilt(butter(2, 3000, btype="high", fs=SR, output="sos"), reference) * 0.15 + reference * 0.85
    reference /= max(1e-9, np.max(np.abs(reference))) / 0.9

    (OUT / "vocal").mkdir(parents=True, exist_ok=True)
    (OUT / "beat").mkdir(parents=True, exist_ok=True)
    (OUT / "reference").mkdir(parents=True, exist_ok=True)

    sf.write(OUT / "vocal" / "synth_vocal.wav", vocal, SR, subtype="PCM_24")
    sf.write(OUT / "beat" / "synth_beat.wav", beat, SR, subtype="PCM_24")
    sf.write(OUT / "reference" / "synth_ref.wav", reference, SR, subtype="PCM_24")

    print("wrote synthetic test files to", OUT)


if __name__ == "__main__":
    main()
