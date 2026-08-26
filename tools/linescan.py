#!/usr/bin/env python3
"""Find 1px horizontal anomalies in a screenshot.

A real drawn/blended hairline shows up as a small but CONSISTENT signed bias
across the whole width, even when each pixel only differs by 2-3 levels.
Texture, by contrast, averages out to ~0 bias.
"""
import sys

import numpy as np
from PIL import Image


def scan(path, top=12):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    lum = a.mean(axis=2)
    h, w = lum.shape

    up = lum[0:h - 2, :]
    mid = lum[1:h - 1, :]
    dn = lum[2:h, :]

    # Mean signed bias of a row vs the average of its two neighbours.
    bias = (mid - (up + dn) / 2.0).mean(axis=1)
    # Fraction of columns where the row is consistently darker/brighter.
    d = mid - (up + dn) / 2.0
    frac_dark = (d < -2.0).mean(axis=1)
    frac_bright = (d > 2.0).mean(axis=1)

    print(f"=== {path}  ({w}x{h})")
    print(f"    mean|bias| over all rows = {np.abs(bias).mean():.4f}")
    order = np.argsort(-np.abs(bias))[:top]
    for i in order:
        y = i + 1
        print(f"    y={y:4d} bias={bias[i]:+7.3f} "
              f"dark%={100*frac_dark[i]:5.1f} bright%={100*frac_bright[i]:5.1f}")
    return bias


if __name__ == "__main__":
    for p in sys.argv[1:]:
        scan(p)
        print()
