# AxmolUO

A reimplementation of the Ultima Online client on the Axmol engine, ported from
[ModernUO-Client](https://github.com/brettmax/ModernUO-Client) (ClassicUO, C#/FNA) to C++,
targeting ModernUO shards and The Second Age era data files.

## Packages

| Path | What it is | Ported from |
|---|---|---|
| `core/` | `uocore`: engine-independent C++20 library. UO file formats, network protocol, game state. | `ClassicUO.IO`, `ClassicUO.Assets`, `ClassicUO.Client/Network`, `ClassicUO.Client/Game` |
| `client/` | The Axmol application: scenes, rendering, input, audio. Links `uocore`. | `ClassicUO.Client` (scenes, UI), `ClassicUO.Renderer` |
| `tests/` | doctest suite for `uocore`, runnable without the engine or UO data. | — |
| `server/` | Server packaging notes (ModernUO stays on .NET; see the file). | — |

`uocore` has no Axmol dependency on purpose: it builds and tests in seconds, it can be reused by
offline converters and headless test clients, and the client only has to turn its decoded
RGBA8 images into `ax::Texture2D`s and its world into nodes.

## Build and test the core

```sh
cmake -S uo -B build/uo -G Ninja
cmake --build build/uo
ctest --test-dir build/uo --output-on-failure
```

## Conversion map

| ClassicUO (C#) | AxmolUO (C++) | Status |
|---|---|---|
| `IO/UOFile*`, `MMFileReader`, `UOFileIndex` | `core/io/UOFile`, `MappedFile` | done (MUL + UOP, uncompressed entries) |
| `IO/StackDataReader/Writer` | `core/io/BinaryReader`, `core/net/PacketWriter` | done |
| `Utility/ClientVersion` | `core/io/ClientVersion` | done |
| `Assets/HuesLoader`, `HuesHelper` | `core/assets/Hues`, `Color` | done |
| `Assets/ArtLoader` | `core/assets/Art` | done |
| `Assets/GumpsLoader` | `core/assets/Gumps` | done (zlib/BWT UOP entries pending) |
| `Assets/TileDataLoader` | `core/assets/TileData` | done (old and High Seas layouts) |
| `Assets/MapLoader` | `core/assets/Map` | done (MUL + UOP; diff patches pending) |
| `Assets/ClilocLoader` | `core/assets/Cliloc` | done (BWT-compressed clilocs pending) |
| `Assets/UOFileManager` | `core/assets/Installation` | done |
| `Network/PacketsTable` | `core/net/PacketTable` | done |
| `Network/Huffman` | `core/net/Huffman` | done (+ encoder for tests) |
| `Network/NetClient`, `Scenes/LoginScene` flow | `core/net/Session`, `PacketFramer` | done (unencrypted, as ModernUO accepts) |
| `Network/OutgoingPackets` | `core/net/OutgoingPackets` | login, walk, speech, clicks |
| `Network/PacketHandlers`, `Game/World`, `Game/GameObjects` | `core/world/` (`uo::world`) | entities, containers, equipment, corpses, speech, effects, targeting, trade, shops, books, party, skills; gumps and walking plug in through hooks |
| `Assets/AnimationsLoader`, `AnimDataLoader` | — | to do |
| `Assets/FontsLoader`, unifont | — | to do |
| `Assets/TexmapsLoader`, `LightsLoader`, `MultiLoader`, `SoundsLoader` | — | to do |
| `Renderer/*`, `Game/Scenes/GameScene` | `client/` | in progress |
| `Game/UI/Gumps/*` | — | to do |
| `Network/Encryption/*` | — | not needed for ModernUO |

Source files derived from ClassicUO keep its BSD-2-Clause license and say which C# file
they came from.
