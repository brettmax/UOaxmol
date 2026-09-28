# uoconvert: UO data to Axmol assets

`uoconvert` reads an Ultima Online client folder (The Second Age era or later, MUL or UOP) and
writes an asset tree in formats Axmol loads natively: sprite sheets as PNG plus TexturePacker
format-3 `.plist` for `ax::SpriteFrameCache`, JSON data tables, chunked map files and Ogg
music. It is built on `uocore`: every UO format is decoded by the same code the client runs,
and the converter only chooses what to export and how to lay it out.

The source folder is only read. The ModernUO server reads the original map, statics,
tiledata and multi files in place, so `uoconvert` refuses an output folder inside it.

## Build and run

```sh
cmake -S uo -B build/uo -G Ninja
cmake --build build/uo
ctest --test-dir build/uo --output-on-failure

build/uo/tools/uoconvert/uoconvert --uo "/path/to/UO" --out build/uo-assets
build/uo/tools/uoconvert/uoconvert --list  # stages
```

| Option | Meaning |
|---|---|
| `--only a,b` / `--skip a,b` | run a subset of stages (`--list` names them) |
| `--overrides <dir>` | the shard's loose `Art/Land`, `Art/Statics`, `Gumps` folders (default: the client folder) |
| `--jobs <n>` | sheet pages rendered in parallel (default: all cores) |
| `--max-size <px>` | sheet page edge, default 2048 |
| `--no-uop`, `--no-verdata` | ignore `*LegacyMUL.uop` / `verdata.mul` |
| `--new-format`, `--old-format` | force the 7.0.9+ or older tiledata/multi layout (detected from `tiledata.mul` by default) |
| `--no-radar` | skip the per-map radar PNGs |
| `--soundfont <sf2>` | General MIDI soundfont; with FluidSynth and `oggenc` or `ffmpeg` on PATH, renders `Music/*.mid` |
| `--fluidsynth <exe>`, `--ogg-encoder <exe>` | override the tools used for MIDI |
| `--spine <dir>` | Spine exports to check and copy |

## Output layout (layout version 1)

```
<out>/
  manifest.json                what ran, counts, verdata patches applied, warnings
  art/
    land.json                  index: land id -> sheet + rect
    land-NNN.png / .plist      44x44 land diamonds, frames "land/0x0003"
    statics.json
    statics-NNN.png / .plist   item art, frames "static/0x0EED" (graphic id, no 0x4000 offset)
  gumps/gumps.json, gumps-NNN.png / .plist        frames "gump/0x0BB8"; index has "aliases"
  texmaps/texmaps.json, texmaps-NNN.png / .plist  frames "texmap/0x0003"; 1 px edge extrusion; "aliases"
  lights/lights.json, lights-NNN.png / .plist     frames "light/<n>"
  hues/
    hues.png                   32 x N RGBA lookup texture: row r is hue r + 1
    hues.json                  [name, tableStart, tableEnd] per hue
    radarcol.png               radarcol.mul as a 256-wide colour strip (index = pixel)
  maps/
    map<N>.uomap               chunked land + statics (below)
    map<N>.json                width, height, chunk grid, static count
    map<N>-radar.png           one pixel per tile, radarcol colours
  data/
    tiledata.json              land [flags, texId, name]; statics [flags, weight, layer, count,
                               animId, hue, lightIndex, height, name]
    multis.json                multi id -> [[graphic, x, y, z, flags], ...]
    animdata.json              graphic -> [[frame offsets], frameInterval, frameStart]
  Music/                       mirrors the client's Music/ folder
    **/*.mp3, Config.txt       copied as is
    **/<name>.ogg              rendered from <name>.mid (44.1 kHz stereo Ogg Vorbis)
    music.json                 track id -> {name, loop}, from Config.txt or ClassicUO's table
  spine/                       validated Spine 4.2/4.3 exports (.json/.skel, .atlas, .png)
```

### Sprite sheets

Each group (land, statics, gumps, texmaps, lights) is packed into as few pages as fit
`--max-size`; a sprite bigger than a page gets a page of its own. Pages are trimmed to their
used area, rounded up to a multiple of 4 so they can later be ASTC/ETC2 4x4 compressed.

- Load a page with `SpriteFrameCache::getInstance()->addSpriteFramesWithFile("art/statics-000.plist")`
  and draw with `Sprite::createWithSpriteFrameName("static/0x0EED")`.
- To load pages lazily, read `<group>.json` first: `frames["<id>"]` gives `sheet` (index into
  `sheets`), `rect` `[x, y, w, h]` in the page, and the frame `name`.
- Pixels are straight (not premultiplied) RGBA8. Transparent pixels are exactly 0; everything else
  has alpha 255. Colours are UO's 15-bit colours expanded with ClassicUO's table, so the 5-bit red
  channel the hue shader needs is recoverable as `round(r * 31 / 255)`.
- Land diamonds: colour 0 inside the diamond is black (opaque); the four corners are transparent.
- Statics are drawn by ClassicUO at `(x - (width >> 1) + 22, y - height + 44)` relative to the tile's
  screen position. No anchor is baked in, so the renderer keeps that rule.
- `aliases` in `gumps.json` come from `gump.def` (`{"<new id>": {"source": id, "hue": h}}`, hue
  applied at draw time by the hue shader) and in `texmaps.json` from `TexTerr.def`.
- Loose shard files (`Art/Land/<id>.art`, `Art/Statics/<id>.art`, `Gumps/<id>.gump`, as
  ModernUO-Client reads them) win over the archives, and `verdata.mul` patches are applied,
  exactly as the client applies them.

### Hues

`hues.png` is the texture a hue shader samples: for a source pixel with 15-bit colour `c` drawn
with hue `h` (1-based), the tinted colour is `hues.png` at column `(c >> 10) & 31`, row `h - 1`.
Partial hues apply this only to grey pixels (R == G == B).

### `.uomap` (version 1)

Little-endian throughout. The facet is cut into 64 x 64-tile chunks, row-major, so a chunk is
one zlib inflate away from drawable data.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `UOMP` |
| 4 | 2 | version (1) |
| 6 | 2 | map index |
| 8 | 4 | width in tiles |
| 12 | 4 | height in tiles |
| 16 | 2 | chunk size (64) |
| 18 | 2 | flags: bit 0 = chunks are zlib streams |
| 20 | 4 | chunks across (`ceil(width / 64)`) |
| 24 | 4 | chunks down |
| 28 | 4 | reserved (0) |
| 32 | 16 x chunks | index, entry `cy * chunksX + cx`: u64 file offset, u32 stored size, u32 raw size |

A chunk, once inflated:

| Size | Field |
|---|---|
| 2 x 4096 | land tile id per cell, index `y * 64 + x` |
| 1 x 4096 | land z (signed) per cell |
| 4 | static count |
| 8 x count | `u8 x, u8 y` (0..63 in the chunk), `i8 z`, `u8 0`, `u16 graphic`, `u16 hue` |

Statics are sorted by `(y, x)` and keep the source file's order within a cell, which is the
order the client's depth sort expects as its tie-break. Cells past the facet's edge (1448-wide
maps do not fill the last chunk) hold tile 0, z 0. Map 0 gets verdata land and statics patches.
A missing `map1` is not faked from `map0`; the client aliases it, as ClassicUO does.

A reference reader lives in `uo/tools/uoconvert/src/UoMap.cpp` (`UoMapReader`); in the client,
`ax::ZipUtils::inflateMemory` does the inflate.

### Music

`Music/*.mid` (the T2A soundtrack) becomes `Music/<same name>.ogg`, rendered with FluidSynth
through the soundfont passed as `--soundfont`, then encoded with `oggenc -q 5` or ffmpeg's
libvorbis. No soundfont ships in the repository; FluidR3_GM (MIT) or GeneralUser GS both work.
Without the tools the MIDI files are listed in the manifest as pending. Digital music (`.mp3`)
is copied untouched, since Axmol's AudioEngine plays it.

## Not converted here, and why

| Data | Why |
|---|---|
| Sound effects | Read at runtime by `uo::sound::SoundLoader` (Audio thread); WAV export will reuse it once it lands on this branch. |
| Animations (`anim*.mul`, `AnimationFrame*.uop`) | Owned by the Animations thread; sheet export will call its loader. |
| Fonts (`fonts.mul`, `unifont*.mul`) | Owned by the Fonts thread; BMFont export will call its loader. Axmol's BMFont reader accepts one page per font. |
| Cliloc | The client reads `Cliloc.*` through `uo::assets::Cliloc`; a JSON export needs an iteration accessor there. |
| BWT-wrapped UOP entries (recent clients) | zlib entries are inflated through `uocore`; BWT is not decoded there yet, so those are counted as `compressedSkipped` in the manifest. |
| `MultiCollection.uop`, `mapdif*`/`stadif*` | Not decoded by `uocore` yet; T2A data uses `multi.mul` and verdata. |
