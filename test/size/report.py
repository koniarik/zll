#!/usr/bin/env python3
"""Print the code size (.text + .rodata) of each zll size probe next to baseline.json.

usage: report.py BUILD_DIR [--update]
"""
import argparse
import json
import pathlib
import subprocess

BASELINE = pathlib.Path(__file__).with_name("baseline.json")


def code_size(size_tool, obj):
    out = subprocess.run([size_tool, "-A", obj], capture_output=True, text=True, check=True).stdout
    return sum(
        int(fields[1])
        for fields in (line.split() for line in out.splitlines())
        if len(fields) >= 2 and fields[0].startswith((".text", ".rodata"))
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_dir")
    parser.add_argument("--update", action="store_true", help="store the sizes as the new baseline")
    args = parser.parse_args()

    manifest = json.loads((pathlib.Path(args.build_dir) / "probes.json").read_text())
    sizes = {name: code_size(manifest["size_tool"], obj) for name, obj in manifest["probes"].items()}

    baseline = {"compiler": None, "sizes": {}}
    if BASELINE.exists():
        baseline = json.loads(BASELINE.read_text())
    else:
        print("note: no baseline yet, `make size-baseline` records one")
    if BASELINE.exists() and baseline["compiler"] != manifest["compiler"]:
        print(
            f"note: the baseline is from arm-none-eabi-g++ {baseline['compiler']}, "
            f"this build uses {manifest['compiler']}"
        )

    print(f"{'probe':<8}{'bytes':>8}{'baseline':>10}{'delta':>8}")
    for name, size in sizes.items():
        base = baseline["sizes"].get(name)
        if base is None:
            print(f"{name:<8}{size:>8}{'-':>10}{'-':>8}")
        else:
            print(f"{name:<8}{size:>8}{base:>10}{size - base:>+8}  ({(size - base) / base:+.1%})")

    if args.update:
        content = {"compiler": manifest["compiler"], "sizes": sizes}
        BASELINE.write_text(json.dumps(content, indent=2) + "\n")
        print(f"baseline updated: {BASELINE}")


if __name__ == "__main__":
    main()
