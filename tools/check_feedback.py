#!/usr/bin/env python3
"""Run movement, audio and HUD regression checks using Emscripten and Node."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emcc", default="emcc", help="emcc executable or emcc.py path")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    output = root / "build" / "feedback-tests"
    output.mkdir(parents=True, exist_ok=True)
    compiler = ([sys.executable, args.emcc] if args.emcc.endswith(".py")
                else [shutil.which(args.emcc) or args.emcc])
    node = shutil.which("node")
    if not node:
        parser.error("Node is required to execute the WebAssembly regression checks")
    for name, sources, flags in (
        ("movement", ["src/math_tables.c", "src/level.c"], []),
        ("audio", [], ["-sUSE_SDL=2"]),
        ("hud", [], ["-sUSE_SDL=2"]),
    ):
        target = output / f"{name}.js"
        subprocess.run(compiler + [f"tests/feedback_{name}.c"] + sources +
                       ["-Isrc", "-Ithird_party", "-O2", "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1"] +
                       flags + ["-o", str(target)], cwd=root, check=True)
        subprocess.run([node, str(target)], cwd=root, check=True)


if __name__ == "__main__":
    main()
