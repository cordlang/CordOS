#!/usr/bin/env python3
"""Show the horizontal shape of a row anomaly: which x ranges differ."""
import sys

import numpy as np
from PIL import Image


def runs(mask):
    out = []
    start = None
    for i, v in enumerate(mask):
        if v and start is None:
            start = i
        elif not v and start is not None:
            out.append((start, i - 1))
            start = None
    if start is not None:
        out.append((start, len(mask) - 1))
    return out


def main(path, y):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    h, w, _ = a.shape
    row = a[y]
    ref = (a[y - 1] + a[y + 1]) / 2.0
    d = np.abs(row - ref).sum(axis=1)
    mask = d > 12
    r = runs(mask)
    print(f"=== {path} row y={y}  ({w}px wide)")
    print(f"    columns differing: {int(mask.sum())} / {w} "
          f"({100*mask.sum()//w}%)")
    print(f"    number of differing runs: {len(r)}")
    print(f"    first 25 runs: {r[:25]}")
    if r:
        widths = [b - a2 + 1 for a2, b in r]
        print(f"    run width: min={min(widths)} max={max(widths)} "
              f"mean={sum(widths)/len(widths):.1f}")
        print(f"    span: x[{r[0][0]} .. {r[-1][1]}]")
    # periodicity check: distance between run starts
    if len(r) > 3:
        starts = [a2 for a2, _ in r]
        gaps = [starts[i + 1] - starts[i] for i in range(len(starts) - 1)]
        print(f"    gaps between runs: min={min(gaps)} max={max(gaps)} "
              f"mean={sum(gaps)/len(gaps):.1f}")


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]))
