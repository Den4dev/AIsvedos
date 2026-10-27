#!/usr/bin/env python3
"""Separate vocals from a reference mix with Demucs.

The plugin compares a vocal profile against the reference tilt, so feeding it a
full mix skews the match towards the beat. Run this once per reference track and
load the resulting *_vocals.wav in the plugin instead of the mix.

    python tools/split_reference.py "C:/Music/ref_track.wav"

Requires: pip install -U demucs  (pulls torch, ~2 GB) and ffmpeg on PATH.
"""

import argparse
import importlib.util
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Split a reference mix into a vocal stem with Demucs")
    parser.add_argument("input", help="reference mix (wav/flac/mp3)")
    parser.add_argument("--out", default="test_audio/reference", help="output folder")
    parser.add_argument("--model", default="htdemucs")
    parser.add_argument("--device", default="auto", choices=["auto", "cpu", "cuda", "mps"])
    args = parser.parse_args()

    if importlib.util.find_spec("demucs") is None:
        return fail(
            "Demucs is not installed.\n"
            "  python -m pip install -U demucs\n"
            "That pulls torch (~2 GB). Re-run this script when it finishes."
        )

    source = Path(args.input).expanduser().resolve()

    if not source.is_file():
        return fail(f"File not found: {source}")

    out_dir = Path(args.out).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="vp_demucs_") as temp_dir:
        command = [
            sys.executable, "-m", "demucs",
            "--two-stems=vocals",
            "-n", args.model,
            "-o", temp_dir,
        ]

        if args.device != "auto":
            command += ["--device", args.device]

        command.append(str(source))

        print("Demucs:", " ".join(command))
        print("On CPU this can take several minutes.")

        try:
            subprocess.run(command, check=True)
        except subprocess.CalledProcessError as error:
            return fail(f"Demucs failed with code {error.returncode}. Check ffmpeg and the input file.")
        except FileNotFoundError:
            return fail("Could not launch python to run demucs.")

        stems = sorted(Path(temp_dir).rglob("vocals.wav"))

        if not stems:
            return fail("Demucs produced no vocals.wav, see the log above.")

        destination = out_dir / f"{source.stem}_vocals.wav"
        shutil.copy2(stems[0], destination)

    print(f"\nDone: {destination}")
    print("Load this stem in the plugin as Reference: it no longer carries the full-mix tilt.")
    print("Demucs leaves the track reverb/delay in the stem - expected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
