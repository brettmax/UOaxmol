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

# multi.mul/multi.idx: multi 1 is a ring of crates (art 0x11) around its origin, 12-byte T2A
# records { graphic, x, y, z, flags }. The first record is invisible (flags 0), as in real data.
mparts = [(0x0001, 0, 0, 0, 0)]
mparts += [(0x11, dx, dy, 0, 1) for dx in range(-2, 3) for dy in range(-2, 3) if max(abs(dx), abs(dy)) == 2]
mdata = b"".join(struct.pack("<HhhhI", *p) for p in mparts)
w("multi.mul", mdata)
w("multi.idx", b"\xff\xff\xff\xff\0\0\0\0\0\0\0\0" + struct.pack("<IiI", 0, len(mdata), 0))
# anim.mul/anim.idx: one 20x40 figure (head, red torso, blue legs) in two frames, used for every
# action and direction of the human body 0x190, so the player and NPC are drawn and animate.
# MUL block: 256-colour palette, frame count, offsets (from after the palette), then frames of
# RLE runs whose 10-bit x/y are relative to the frame's centre and bottom.
apal = [0] * 256
apal[1], apal[2], apal[3] = c16(220, 40, 40), c16(40, 80, 220), c16(240, 220, 160)
AW, AH, ACX = 20, 40, 10


def anim_frame(shift):
    f = struct.pack("<hhhh", ACX, 0, AW, AH)
    for r in range(AH):
        colour = 3 if r < 8 else (1 if r < 24 else 2)
        x0, x1 = ((6, 14) if r < 8 else (2, 18))
        x0, x1 = x0 + shift, x1 + shift
        f += struct.pack("<I", (((x0 - ACX) & 0x3FF) << 22) | (((r - AH) & 0x3FF) << 12) | (x1 - x0))
        f += bytes([colour]) * (x1 - x0)
    return f + struct.pack("<I", 0x7FFF7FFF)


aframes = [anim_frame(0), anim_frame(1)]
ablock = b"".join(struct.pack("<H", c) for c in apal) + struct.pack("<I", len(aframes))
aoff, adata, aoffs = 4 + 4 * len(aframes), b"", []
for fr in aframes:
    aoffs.append(aoff + len(adata))
    adata += fr
ablock += b"".join(struct.pack("<I", o) for o in aoffs) + adata
w("anim.mul", ablock)
# People bodies (0x190+) start at block 35000, 35 actions x 5 directions each.
aidx = bytearray(b"\xff\xff\xff\xff\0\0\0\0\0\0\0\0" * 35000)
aidx += struct.pack("<IIi", 0, len(ablock), 0) * 175
w("anim.idx", bytes(aidx))

print("wrote", out)
