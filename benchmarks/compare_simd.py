#!/usr/bin/env python3
# benchmarks/compare_simd.py — side-by-side speedup from two bench JSONs
#
# Used by bench_simd.{bat,sh}. Stdlib-only so it works without scipy
# (which Google Benchmark's own compare.py requires). Given two
# --benchmark_out=json files it prints one row per shared benchmark
# with baseline / SIMD nanoseconds and the speedup ratio.

import json
import sys


def load(path):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    return {b["name"]: b for b in data.get("benchmarks", []) if "real_time" in b}


def fmt_ns(ns):
    if ns >= 1e9:
        return f"{ns / 1e9:8.2f} s"
    if ns >= 1e6:
        return f"{ns / 1e6:8.2f} ms"
    if ns >= 1e3:
        return f"{ns / 1e3:8.2f} us"
    return f"{ns:8.0f} ns"


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} baseline.json simd.json", file=sys.stderr)
        return 2

    baseline = load(argv[1])
    simd = load(argv[2])
    shared = sorted(set(baseline) & set(simd))
    if not shared:
        print("No shared benchmark names between the two files.", file=sys.stderr)
        return 1

    name_w = max(len(n) for n in shared)
    header = f"{'benchmark'.ljust(name_w)}  {'baseline':>12}  {'simd':>12}  {'speedup':>8}"
    print(header)
    print("-" * len(header))

    geomean_log = 0.0
    geomean_cnt = 0
    for name in shared:
        b = baseline[name]["real_time"]
        s = simd[name]["real_time"]
        if b <= 0 or s <= 0:
            continue
        speedup = b / s
        print(f"{name.ljust(name_w)}  {fmt_ns(b):>12}  {fmt_ns(s):>12}  {speedup:>7.2f}x")
        import math
        geomean_log += math.log(speedup)
        geomean_cnt += 1

    if geomean_cnt:
        import math
        gm = math.exp(geomean_log / geomean_cnt)
        print("-" * len(header))
        print(f"{'geometric mean'.ljust(name_w)}  {'':>12}  {'':>12}  {gm:>7.2f}x")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
