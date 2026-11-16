"""Frozen A/3 coarse-count exception and fixed scalar-coordinate identity."""

import math

DMAX = 8736
DIMENSIONS = [1092, 2184, 4368, 8736]
LEFT = float.fromhex("0x1.0006f5248e2e8p-4")
RIGHT = float.fromhex("0x1.2006f5248e2e8p-4")


def coarse_count(index, left, right):
    matching = left == LEFT and right == RIGHT
    if index == 60:
        if not matching or math.ceil(384 * (right - left)) != 3:
            raise ValueError("A/3 segment 60 endpoint/count mismatch")
        return 4
    if matching:
        raise ValueError("A/3 exception at wrong segment index")
    return math.ceil(384 * (right - left))
