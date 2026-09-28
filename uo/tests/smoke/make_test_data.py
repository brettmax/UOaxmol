#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Writes a tiny synthetic UO data folder: enough hues, tiledata, art and map for the client
to log in and draw a patch of Britannia around (1440, 1690). Not real UO art; for smoke tests
and CI, where the real (copyrighted) data files cannot be used."""
import os
import struct
import sys

out = sys.argv[1] if len(sys.argv) > 1 else "testdata"
os.makedirs(out, exist_ok=True)


def w(name, data):
    with open(os.path.join(out, name), "wb") as f:
        f.write(data)


def c16(r, g, b):
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


# hues.mul: one group of 8 hues, each a ramp of its colour.
hues = bytearray(struct.pack("<I", 0))
for e in range(8):
    for i in range(32):
        hues += struct.pack("<H", c16(8 * i, 4 * i, 255 - 8 * i))
    hues += struct.pack("<HH", 0, 0) + (b"hue%d" % e).ljust(20, b"\0")
w("hues.mul", bytes(hues))
w("radarcol.mul", b"\0\0" * 0x14000)

# tiledata.mul, pre-High Seas layout: 512 land groups, 64 static groups.
td = bytearray()
for g in range(512):
    td += struct.pack("<I", 0)
    for j in range(32):
        td += struct.pack("<IH", 0, 0) + b"land".ljust(20, b"\0")
for g in range(64):
    td += struct.pack("<I", 0)
    for j in range(32):
        td += struct.pack("<IBBiHHHB", 0, 0, 0, 0, 0, 0, 0, 10) + b"static".ljust(20, b"\0")
w("tiledata.mul", bytes(td))

# art.mul: land tiles 3 (grass) and 4 (dirt), statics 0x10 (tree) and 0x11 (crate).
art = bytearray()
idx = bytearray(b"\xff\xff\xff\xff\0\0\0\0\0\0\0\0" * 0x4020)


def put(index, data):
    global art
    struct.pack_into("<IiI", idx, index * 12, len(art), len(data), 0)
    art += data


def land(color):
    return struct.pack("<H", color) * 1012


def static(width, height, pixel):
    rows = []
    for y in range(height):
        row = bytearray()
        row += struct.pack("<HH", 0, width)
        row += b"".join(struct.pack("<H", pixel(x, y)) for x in range(width))
        row += struct.pack("<HH", 0, 0)
        rows.append(row)
    table = bytearray()
    offset = 0
    for r in rows:
        table += struct.pack("<H", offset)
        offset += len(r) // 2
    return struct.pack("<IHH", 0, width, height) + table + b"".join(rows)


put(3, land(c16(60, 140, 50)))
put(4, land(c16(130, 100, 60)))
put(0x4000 + 0x10, static(30, 70, lambda x, y: c16(30, 110, 40) if y < 45 else c16(100, 70, 40)))
put(0x4000 + 0x11, static(26, 26, lambda x, y: c16(170, 130, 70) if 2 < x < 23 else c16(90, 60, 30)))
w("art.mul", bytes(art))
w("artidx.mul", bytes(idx))
w("gumpart.mul", b"")
w("gumpidx.mul", b"")

# map0.mul: T2A-sized facet (6144 x 4096), only the blocks around the player filled in.
BW, BH = 768, 512
px, py = 1440, 1690
blocks = {}
statics = {}
for bx in range(px // 8 - 4, px // 8 + 5):
    for by in range(py // 8 - 4, py // 8 + 5):
        cells = b""
        for cy in range(8):
            for cx in range(8):
                x, y = bx * 8 + cx, by * 8 + cy
                tile = 4 if (x + y) % 11 == 0 or abs(x - px) + abs(y - py) < 3 else 3
                cells += struct.pack("<Hb", tile, 0)
        blocks[bx * BH + by] = struct.pack("<I", 0) + cells
for (x, y, g) in [(1436, 1686, 0x10), (1444, 1688, 0x10), (1442, 1694, 0x11), (1437, 1693, 0x10)]:
    statics.setdefault((x // 8) * BH + y // 8, []).append(struct.pack("<HBBbH", g, x % 8, y % 8, 0, 0))

last = max(blocks)
with open(os.path.join(out, "map0.mul"), "wb") as f:
    for b in range(last + 1):
        f.write(blocks.get(b, struct.pack("<I", 0) + struct.pack("<Hb", 0x0002, 0) * 64))
    # pad to the full T2A size so the client derives a 6144-wide facet
    f.truncate(BW * BH * 196)

staidx = bytearray(b"\xff\xff\xff\xff\0\0\0\0\0\0\0\0" * (BW * BH))
sdata = bytearray()
for b, items in statics.items():
    data = b"".join(items)
    struct.pack_into("<IiI", staidx, b * 12, len(sdata), len(data), 0)
    sdata += data
w("staidx0.mul", bytes(staidx))
w("statics0.mul", bytes(sdata))
print("wrote", out)
