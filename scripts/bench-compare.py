#!/usr/bin/env python3
"""
Compare the three render backends' benchmark runs.

Each backend's build writes /dev_hdd0/tmp/bench_<backend>.csv while running the
in-game BENCHMARK scene. On macOS RPCS3 maps that to
  ~/Library/Application Support/rpcs3/dev_hdd0/tmp/bench_<backend>.csv

Usage:
  python3 scripts/bench-compare.py                 # auto-find the three CSVs
  python3 scripts/bench-compare.py a.csv b.csv ...  # explicit files

Prints a per-stage avg-FPS table (tiny3d / raylib / rsxgl side by side, fastest
marked) and writes a merged bench-comparison.csv next to the inputs / cwd.

Note: numbers are measured under RPCS3, not a real PS3 — valid as a RELATIVE
comparison of the render paths, not as absolute PS3 performance.
"""
import csv
import glob
import os
import sys

BACKENDS = ["tiny3d", "raylib", "rsxgl"]
RPCS3_TMP = os.path.expanduser(
    "~/Library/Application Support/rpcs3/dev_hdd0/tmp"
)


def find_csvs(args):
    if args:
        return args
    found = []
    for pat in (
        os.path.join(RPCS3_TMP, "bench_*.csv"),
        os.path.expanduser("~/Library/Application Support/rpcs3/dev_hdd0/bench_*.csv"),
        "bench_*.csv",
    ):
        found += glob.glob(pat)
    # de-dupe, keep first occurrence
    seen, uniq = set(), []
    for f in found:
        b = os.path.basename(f)
        if b not in seen:
            seen.add(b)
            uniq.append(f)
    return uniq


def load(path):
    with open(path, newline="") as fh:
        rows = list(csv.DictReader(fh))
    for r in rows:
        for k in ("stage", "target", "live", "frames", "spawns", "destroys"):
            r[k] = int(float(r[k]))
        for k in ("avg_ms", "avg_fps", "min_fps", "max_fps"):
            r[k] = float(r[k])
    return rows


def main():
    files = find_csvs(sys.argv[1:])
    if not files:
        print("No bench_*.csv found. Run the BENCHMARK scene on each build first,")
        print(f"or pass paths explicitly. Looked in:\n  {RPCS3_TMP}")
        return 1

    data = {}          # backend -> {target: row}
    targets = []       # ordered unique stage targets
    for f in files:
        rows = load(f)
        if not rows:
            continue
        be = rows[0]["backend"]
        data[be] = {r["target"]: r for r in rows}
        for r in rows:
            if r["target"] not in targets:
                targets.append(r["target"])
    targets.sort()

    present = [b for b in BACKENDS if b in data] + \
              [b for b in data if b not in BACKENDS]
    if not present:
        print("No usable rows.")
        return 1

    # ---- avg-FPS table ----
    print("\nAverage FPS per stage (higher = better). '*' = fastest at that load.")
    print("Measured under RPCS3 — relative comparison, not real-PS3 numbers.\n")
    header = f"{'sprites':>8} | " + " | ".join(f"{b:>10}" for b in present)
    print(header)
    print("-" * len(header))
    for t in targets:
        cells = []
        best = max(
            (data[b][t]["avg_fps"] for b in present if t in data[b]),
            default=0.0,
        )
        for b in present:
            if t in data[b]:
                v = data[b][t]["avg_fps"]
                mark = "*" if abs(v - best) < 1e-6 else " "
                cells.append(f"{v:>9.1f}{mark}")
            else:
                cells.append(f"{'-':>10}")
        print(f"{t:>8} | " + " | ".join(cells))

    # ---- relative-to-fastest ratios ----
    print("\nRelative to the fastest backend at each load (1.00 = fastest):\n")
    print(header)
    print("-" * len(header))
    for t in targets:
        best = max(
            (data[b][t]["avg_fps"] for b in present if t in data[b]),
            default=0.0,
        )
        cells = []
        for b in present:
            if t in data[b] and best > 0:
                cells.append(f"{data[b][t]['avg_fps'] / best:>10.2f}")
            else:
                cells.append(f"{'-':>10}")
        print(f"{t:>8} | " + " | ".join(cells))

    # ---- merged CSV ----
    out = "bench-comparison.csv"
    with open(out, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["target"] + [f"{b}_avg_fps" for b in present] +
                   [f"{b}_avg_ms" for b in present])
        for t in targets:
            row = [t]
            row += [data[b][t]["avg_fps"] if t in data[b] else "" for b in present]
            row += [data[b][t]["avg_ms"] if t in data[b] else "" for b in present]
            w.writerow(row)
    print(f"\nMerged -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
