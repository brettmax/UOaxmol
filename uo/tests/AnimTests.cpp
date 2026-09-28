// SPDX-License-Identifier: BSD-2-Clause
// Tests for uo::anim: the animation loader (MUL, verdata, UOP, BWT), the frame cache,
// action selection, equipment ordering and the mobile draw list. Every fixture is built
// here byte by byte, so the suite needs no UO installation.
#include "TestUtil.h"
#include "doctest.h"

#include "uo/anim/AnimationCache.h"
#include "uo/anim/AnimationsLoader.h"
#include "uo/anim/Equipment.h"
#include "uo/anim/MobileAnimation.h"
#include "uo/anim/MobileRenderer.h"
#include "uo/assets/Verdata.h"
#include "uo/assets/Bwt.h"
#include "uo/io/Compression.h"
#include "uo/io/DefReader.h"
#include "uo/io/UOFile.h"


#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace uo;
using namespace uo::anim;
using namespace uotest;

namespace
{

constexpr uint16_t RED   = 0x7C00;
constexpr uint16_t GREEN = 0x03E0;
constexpr uint16_t BLUE  = 0x001F;

// RGBA8 (R in the low byte) of the three palette colours, opaque.
constexpr uint32_t RED32   = 0xFF0000FFu;
constexpr uint32_t GREEN32 = 0xFF00FF00u;
constexpr uint32_t BLUE32  = 0xFFFF0000u;

// A sprite as palette indices; index 0 is left out of the runs (transparent).
struct Sprite
{
    int16_t cx = 0;
    int16_t cy = 0;
    int16_t w  = 0;
    int16_t h  = 0;
    std::vector<uint8_t> px; // w * h

    static Sprite solid(int16_t w, int16_t h, uint8_t index, int16_t cx = 0, int16_t cy = 0)
    {
        Sprite s{cx, cy, w, h, std::vector<uint8_t>(size_t(w) * h, index)};
        return s;
    }
};

std::array<uint16_t, 256> palette()
{
    std::array<uint16_t, 256> p{};
    p[1] = RED;
    p[2] = GREEN;
    p[3] = BLUE;
    return p;
}

void appendPalette(Bytes& b)
{
    for (uint16_t c : palette())
    {
        le16(b, c);
    }
}

void appendSprite(Bytes& b, const Sprite& s)
{
    le16(b, static_cast<uint16_t>(s.cx));
    le16(b, static_cast<uint16_t>(s.cy));
    le16(b, static_cast<uint16_t>(s.w));
    le16(b, static_cast<uint16_t>(s.h));

    for (int y = 0; y < s.h; ++y)
    {
        int x = 0;
        while (x < s.w)
        {
            if (s.px[size_t(y) * s.w + x] == 0)
            {
                ++x;
                continue;
            }
            int end = x;
            while (end < s.w && s.px[size_t(y) * s.w + end] != 0)
            {
                ++end;
            }
            const int rx = x - s.cx;
            const int ry = y - s.cy - s.h;
            le32(b, (uint32_t(rx & 0x3FF) << 22) | (uint32_t(ry & 0x3FF) << 12) | uint32_t(end - x));
            for (int k = x; k < end; ++k)
            {
                b.push_back(s.px[size_t(y) * s.w + k]);
            }
            x = end;
        }
    }
    le32(b, 0x7FFF7FFF);
}

// One anim.mul block: palette, frame count, frame offsets, frames.
Bytes mulBlock(const std::vector<Sprite>& frames)
{
    Bytes b;
    appendPalette(b);
    std::vector<Bytes> encoded;
    for (const auto& f : frames)
    {
        Bytes e;
        appendSprite(e, f);
        encoded.push_back(std::move(e));
    }
    le32(b, static_cast<uint32_t>(frames.size()));
    uint32_t offset = 4 + 4 * static_cast<uint32_t>(frames.size());
    for (const auto& e : encoded)
    {
        le32(b, offset);
        offset += static_cast<uint32_t>(e.size());
    }
    for (const auto& e : encoded)
    {
        b.insert(b.end(), e.begin(), e.end());
    }
    return b;
}

// Builds anim.idx / anim.mul pairs block by block.
struct MulAnimFile
{
    std::map<uint32_t, Bytes> blocks; // block number -> data
    Bytes mul{0, 0, 0, 0};            // offset 0 is "no data", keep real blocks off it

    static uint32_t peopleBlock(uint16_t body, int action, int dir) { return (body - 400) * 175 + 35000 + action * 5 + dir; }
    static uint32_t highBlock(uint16_t body, int action, int dir) { return body * 110 + action * 5 + dir; }
    static uint32_t lowBlock(uint16_t body, int action, int dir) { return (body - 200) * 65 + 22000 + action * 5 + dir; }

    void set(uint32_t block, const std::vector<Sprite>& frames) { blocks[block] = mulBlock(frames); }

    void write(const TempDir& dir, const std::string& name = "anim") const
    {
        uint32_t maxBlock = 0;
        for (const auto& [k, v] : blocks)
        {
            maxBlock = std::max(maxBlock, k);
        }
        // Room for a whole people body after the last block, so range checks pass.
        const uint32_t count = maxBlock + 175 * 5;

        Bytes idx;
        Bytes data = mul;
        std::map<uint32_t, std::pair<uint32_t, uint32_t>> at;
        for (const auto& [k, v] : blocks)
        {
            at[k] = {static_cast<uint32_t>(data.size()), static_cast<uint32_t>(v.size())};
            data.insert(data.end(), v.begin(), v.end());
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            if (auto it = at.find(i); it != at.end())
            {
                le32(idx, it->second.first);
                le32(idx, it->second.second);
                le32(idx, 0);
            }
            else
            {
                le32(idx, 0xFFFFFFFF);
                le32(idx, 0);
                le32(idx, 0);
            }
        }
        dir.write(name + ".idx", idx);
        dir.write(name + ".mul", data);
    }
};

Bytes zlibCompress(const Bytes& in)
{
    Bytes out = io::deflate(in);
    REQUIRE(!out.empty());
    return out;
}

struct UopEntrySpec
{
    std::string name;
    Bytes stored;
    int16_t flag              = 0;
    uint32_t decompressedSize = 0;
};

Bytes uopArchive(const std::vector<UopEntrySpec>& entries)
{
    Bytes f;
    le32(f, 0x50594D);
    le32(f, 5);
    le32(f, 0xFD23EC43);
    le64(f, 28); // first block right after the header
    le32(f, 1000);
    le32(f, static_cast<uint32_t>(entries.size()));

    le32(f, static_cast<uint32_t>(entries.size()));
    le64(f, 0);

    uint64_t dataAt = f.size() + entries.size() * 34;
    for (const auto& e : entries)
    {
        le64(f, dataAt);
        le32(f, 0); // header length
        le32(f, static_cast<uint32_t>(e.stored.size()));
        le32(f, e.flag == 0 ? static_cast<uint32_t>(e.stored.size()) : e.decompressedSize);
        le64(f, io::UopFile::hash(e.name));
        le32(f, 0);
        le16(f, static_cast<uint16_t>(e.flag));
        dataAt += e.stored.size();
    }
    for (const auto& e : entries)
    {
        f.insert(f.end(), e.stored.begin(), e.stored.end());
    }
    return f;
}

struct UopFrameSpec
{
    uint16_t group;
    uint16_t frameId; // 1-based, runs across the five directions
    Sprite sprite;
};

// The decompressed layout of one animationlegacyframe entry.
Bytes uopAnimBlock(const std::vector<UopFrameSpec>& frames)
{
    Bytes b(32, 0);
    le32(b, static_cast<uint32_t>(frames.size()));
    le32(b, 40); // frame headers start right here

    const size_t headersAt = b.size();
    b.resize(headersAt + frames.size() * 16, 0);

    for (size_t i = 0; i < frames.size(); ++i)
    {
        const size_t headerPos = headersAt + i * 16;
        const size_t pixelPos  = b.size();

        Bytes h;
        le16(h, frames[i].group);
        le16(h, frames[i].frameId);
        le64(h, 0);
        le32(h, static_cast<uint32_t>(pixelPos - headerPos));
        std::copy(h.begin(), h.end(), b.begin() + static_cast<std::ptrdiff_t>(headerPos));

        appendPalette(b);
        appendSprite(b, frames[i].sprite);
    }
    return b;
}

std::string uopFrameName(int body, int action)
{
    char name[64];
    std::snprintf(name, sizeof(name), "build/animationlegacyframe/%06d/%02d.bin", body, action);
    return name;
}

// Encoder for the BWT stage bwtDecompress inverts: a move-to-position list code over the
// symbols ordered by next use, then a move-to-front pass.
Bytes bwtEncode(const Bytes& payload)
{
    std::array<int32_t, 256> counts{};
    for (uint8_t b : payload)
    {
        ++counts[b];
    }

    // Descending count, lower symbol first on ties (the decoder's order).
    std::vector<int> order;
    std::array<int32_t, 256> tmp = counts;
    for (;;)
    {
        int best = -1;
        for (int j = 0; j < 256; ++j)
        {
            if (tmp[j] > 0 && (best < 0 || tmp[j] > tmp[best]))
            {
                best = j;
            }
        }
        if (best < 0)
        {
            break;
        }
        order.push_back(best);
        tmp[best] = 0;
    }

    std::array<std::vector<size_t>, 256> occ;
    for (size_t t = 0; t < payload.size(); ++t)
    {
        occ[payload[t]].push_back(t);
    }

    std::vector<int> list;
    for (size_t t = 0; t < payload.size(); ++t)
    {
        if (std::find(list.begin(), list.end(), payload[t]) == list.end())
        {
            list.push_back(payload[t]);
        }
    }

    std::array<std::vector<uint8_t>, 256> seg;
    for (size_t i = 0; i < list.size(); ++i)
    {
        seg[list[i]].push_back(static_cast<uint8_t>(i));
    }

    std::array<size_t, 256> used{};
    auto nextOcc = [&](int s) { return occ[s][used[s]]; };

    for (size_t t = 0; t < payload.size(); ++t)
    {
        const int v = payload[t];
        REQUIRE(list.front() == v);
        list.erase(list.begin());
        ++used[v];
        if (used[v] < occ[v].size())
        {
            const size_t when = nextOcc(v);
            size_t idx        = 0;
            while (idx < list.size() && nextOcc(list[idx]) < when)
            {
                ++idx;
            }
            seg[v].push_back(static_cast<uint8_t>(idx));
            list.insert(list.begin() + static_cast<std::ptrdiff_t>(idx), v);
        }
    }

    Bytes transformed;
    for (int32_t c : counts)
    {
        le32(transformed, static_cast<uint32_t>(c));
    }
    for (int s : order)
    {
        transformed.insert(transformed.end(), seg[s].begin(), seg[s].end());
    }

    Bytes out{0, 0, 0, 0};
    std::array<uint8_t, 256> mtf{};
    for (int i = 0; i < 256; ++i)
    {
        mtf[i] = static_cast<uint8_t>(i);
    }
    for (uint8_t v : transformed)
    {
        const auto it  = std::find(mtf.begin(), mtf.end(), v);
        const auto pos = static_cast<uint8_t>(it - mtf.begin());
        out.push_back(pos);
        std::rotate(mtf.begin(), it, it + 1);
    }
    out.push_back(0); // read by the decoder but never emitted
    return out;
}

AnimationsConfig configFor(const TempDir& dir)
{
    AnimationsConfig c;
    c.directory = dir.path.string();
    c.version   = cv::CV_7000;
    return c;
}

} // namespace

TEST_CASE("def reader keeps ClassicUO's tokenizing rules")
{
    auto r = io::DefReader::fromString("# comment\n"
                                       "400 {401, 402, 403} 33 # trailing\n"
                                       "x ignored line\n"
                                       "12\n"
                                       "7 0x10 {0x20, 5}\n");
    REQUIRE(r.next());
    CHECK(r.partsCount() == 3);
    CHECK(r.readInt() == 400);
    auto group = r.readGroup();
    REQUIRE(group);
    CHECK(*group == std::vector<int>{401, 402, 403});
    CHECK(r.readInt() == 33);

    // "12" alone has fewer than the default two tokens and is dropped.
    REQUIRE(r.next());
    CHECK(r.readInt() == 7);
    CHECK(r.readInt() == 0); // hex is cut at the 'x', as ClassicUO does
    group = r.readGroup();
    REQUIRE(group);
    CHECK(*group == std::vector<int>{5}); // "0x20" fails ClassicUO's HexNumber parse and is dropped
    CHECK_FALSE(r.next());

    auto g = io::DefReader::fromString("3 {9}\n");
    REQUIRE(g.next());
    CHECK(g.readInt() == 3);
    CHECK(g.readGroupInt() == 9);
}

TEST_CASE("bwt stage inverts a move-to-position + move-to-front encoding")
{
    Bytes payload;
    for (int i = 0; i < 3000; ++i)
    {
        payload.push_back(static_cast<uint8_t>((i * 7) % 13 + (i % 5 == 0 ? 200 : 0)));
    }
    payload.push_back(42);

    const Bytes encoded = bwtEncode(payload);
    CHECK(assets::bwtDecompress(encoded) == payload);

    const Bytes single{9, 9, 9, 9};
    CHECK(assets::bwtDecompress(bwtEncode(single)) == single);

    CHECK(assets::bwtDecompress(Bytes{1, 2, 3}).empty());
    CHECK(assets::bwtDecompress(Bytes(20, 0)).empty());
}

TEST_CASE("anim.mul frames decode with palette, centre and transparency")
{
    TempDir dir;
    MulAnimFile anim;

    Sprite s = Sprite::solid(4, 3, 1, 2, -1);
    s.px[1]  = 0; // a hole in the first row
    s.px[5]  = 2;
    s.px[11] = 3;
    anim.set(MulAnimFile::peopleBlock(400, 4, 0), {s, Sprite::solid(2, 2, 3)});
    anim.write(dir);

    AnimationsLoader loader;
    REQUIRE(loader.load(configFor(dir)));

    auto indices = loader.getIndices(400);
    CHECK(indices.type == AnimationGroupsType::Human);
    CHECK(indices.fileIndex == 0);
    REQUIRE(indices.directions.size() == size_t(PeopleAnimationGroup::AnimationCount) * MAX_DIRECTIONS);

    AnimationCache cache(loader);
    auto result = cache.getAnimationFrames(400, 4, 0);
    REQUIRE(result.frames.size() == 2);
    CHECK_FALSE(result.isUop);

    const Frame& f = result.frames[0];
    CHECK(f.width == 4);
    CHECK(f.height == 3);
    CHECK(f.centerX == 2);
    CHECK(f.centerY == -1);
    CHECK(f.pixels[0] == RED32);
    CHECK(f.pixels[1] == 0);
    CHECK(f.pixels[5] == GREEN32);
    CHECK(f.pixels[11] == BLUE32);
    CHECK(f.hitTest(0, 0));
    CHECK_FALSE(f.hitTest(1, 0));
    CHECK_FALSE(f.hitTest(4, 0));

    CHECK(result.frames[1].pixels[3] == BLUE32);

    // A direction with no block, and an action past the table.
    CHECK(cache.getAnimationFrames(400, 4, 1).frames.empty());
    CHECK(cache.getAnimationFrames(400, 79, 0).frames.empty());
    CHECK(cache.getAnimationFrames(400, 80, 0).frames.empty());
    CHECK(cache.animationExists(400, 4));
    CHECK_FALSE(cache.animationExists(400, 5));

    // Frames are cached: the same storage comes back.
    CHECK(cache.getAnimationFrames(400, 4, 0).frames.data() == result.frames.data());
}

TEST_CASE("body.def and bodyconv.def reroute bodies")
{
    TempDir dir;
    MulAnimFile anim;
    anim.set(MulAnimFile::peopleBlock(400, 4, 0), {Sprite::solid(1, 1, 1)});
    anim.set(MulAnimFile::highBlock(3, 1, 0), {Sprite::solid(1, 1, 2)});
    anim.write(dir);

    // anim3 holds body 5; in anim3 bodies below 300 are animals (low group).
    MulAnimFile anim3;
    anim3.set(MulAnimFile::lowBlock(5, 2, 0), {Sprite::solid(1, 1, 3)});
    anim3.write(dir, "anim3");

    dir.write("Body.def", Bytes{'5', '0', '0', ' ', '{', '4', '0', '0', '}', ' ', '3', '3', '\n'});
    const std::string bodyconv = "100\t-1\t5\n";
    dir.write("Bodyconv.def", Bytes(bodyconv.begin(), bodyconv.end()));

    AnimationsLoader loader;
    REQUIRE(loader.load(configFor(dir)));
    AnimationCache cache(loader);

    auto swapped = cache.getAnimationFrames(500, 4, 0);
    REQUIRE(swapped.frames.size() == 1);
    CHECK(swapped.hue == 33);
    CHECK(swapped.frames[0].pixels[0] == RED32);

    uint16_t g = 500;
    cache.convertBodyIfNeeded(g);
    CHECK(g == 400);

    // Without the Anim2 expansion flag the conversion to anim3 (column 2) is refused...
    cache.updateAnimationTable(BodyConvFlags::Anim1);
    CHECK(cache.getAnimationFrames(100, 2, 0).frames.empty());

    // ...and with it, body 100 draws body 5 of anim3 as an animal.
    cache.updateAnimationTable(BodyConvFlags::Anim1 | BodyConvFlags::Anim2);
    auto conv = cache.getAnimationFrames(100, 2, 0);
    REQUIRE(conv.frames.size() == 1);
    CHECK(conv.frames[0].pixels[0] == BLUE32);
    CHECK(cache.getAnimType(100) == AnimationGroupsType::Animal);

    CHECK(cache.getAnimationFrames(3, 1, 0).frames.size() == 1);
    CHECK(cache.getAnimType(3) == AnimationGroupsType::Monster);
}

TEST_CASE("verdata.mul replaces and deletes anim.mul blocks")
{
    TempDir dir;
    MulAnimFile anim;
    anim.set(MulAnimFile::peopleBlock(400, 4, 0), {Sprite::solid(1, 1, 1)});
    anim.set(MulAnimFile::peopleBlock(400, 4, 1), {Sprite::solid(1, 1, 1)});
    anim.write(dir);

    const Bytes patchData = mulBlock({Sprite::solid(3, 1, 2)});
    Bytes verdata;
    le32(verdata, 2);
    const uint32_t dataAt = 4 + 2 * 20;
    // replace direction 0
    le32(verdata, 6);
    le32(verdata, MulAnimFile::peopleBlock(400, 4, 0));
    le32(verdata, dataAt);
    le32(verdata, static_cast<uint32_t>(patchData.size()));
    le32(verdata, 0);
    // delete direction 1
    le32(verdata, 6);
    le32(verdata, MulAnimFile::peopleBlock(400, 4, 1));
    le32(verdata, 0);
    le32(verdata, 0);
    le32(verdata, 0);
    verdata.insert(verdata.end(), patchData.begin(), patchData.end());

    assets::Verdata ver;
    REQUIRE(ver.load(dir.write("verdata.mul", verdata)));
    CHECK(ver.patches().size() == 2);

    auto config    = configFor(dir);
    config.verdata = &ver;
    AnimationsLoader loader;
    REQUIRE(loader.load(config));
    AnimationCache cache(loader);

    auto patched = cache.getAnimationFrames(400, 4, 0);
    REQUIRE(patched.frames.size() == 1);
    CHECK(patched.frames[0].width == 3);
    CHECK(patched.frames[0].pixels[2] == GREEN32);
    CHECK(cache.getAnimationFrames(400, 4, 1).frames.empty());
}

TEST_CASE("UOP animations split by direction, fill gaps and honour AnimationSequence")
{
    TempDir dir;
    constexpr int body = 1000;

    // Action 7: two frames per direction, frame 2 of direction 1 missing.
    std::vector<UopFrameSpec> frames;
    for (int d = 0; d < MAX_DIRECTIONS; ++d)
    {
        for (int i = 0; i < 2; ++i)
        {
            const uint16_t id = static_cast<uint16_t>(d * 2 + i + 1);
            if (id == 4)
            {
                continue;
            }
            frames.push_back({7, id, Sprite::solid(static_cast<int16_t>(d + 1), 2, static_cast<uint8_t>(1 + (i % 3)))});
        }
    }
    const Bytes raw   = uopAnimBlock(frames);
    const Bytes zipped = zlibCompress(raw);

    // Action 9 is BWT + zlib coded.
    const Bytes rawBwt = uopAnimBlock({{9, 1, Sprite::solid(1, 1, 3)},
                                       {9, 2, Sprite::solid(1, 1, 3)},
                                       {9, 3, Sprite::solid(1, 1, 3)},
                                       {9, 4, Sprite::solid(1, 1, 3)},
                                       {9, 5, Sprite::solid(2, 1, 2)}});
    const Bytes bwt    = zlibCompress(bwtEncode(rawBwt));

    dir.write("AnimationFrame1.uop",
              uopArchive({{uopFrameName(body, 7), zipped, 1, static_cast<uint32_t>(raw.size())},
                          {uopFrameName(body, 9), bwt, 3, static_cast<uint32_t>(bwtEncode(rawBwt).size())}}));

    // AnimationSequence: action 5 of this body is drawn with action 7's frames.
    Bytes seq;
    le32(seq, body);
    seq.resize(seq.size() + 48, 0);
    le32(seq, 1);
    le32(seq, 5); // old group
    le32(seq, 0); // frame count 0 = replaced
    le32(seq, 7); // new group
    seq.resize(seq.size() + 60, 0);
    char seqName[64];
    std::snprintf(seqName, sizeof(seqName), "build/animationsequence/%08u.bin", 3u);
    dir.write("AnimationSequence.uop", uopArchive({{seqName, seq, 0, 0}}));

    const std::string mobtypes = "# comment\n1000\tANIMAL\t10000\t# uop animal\n";
    dir.write("mobtypes.txt", Bytes(mobtypes.begin(), mobtypes.end()));

    AnimationsLoader loader;
    REQUIRE(loader.load(configFor(dir)));
    AnimationCache cache(loader);

    CHECK(cache.getAnimFlags(body) == (0x80000000u | AnimationFlags::UseUopAnimation));
    CHECK(cache.getAnimType(body) == AnimationGroupsType::Animal);

    for (int d = 0; d < MAX_DIRECTIONS; ++d)
    {
        auto r = cache.getAnimationFrames(body, 7, static_cast<uint8_t>(d));
        CHECK(r.isUop);
        REQUIRE(r.frames.size() == 2);
        CHECK(r.frames[0].width == d + 1);
        CHECK(r.frames[0].pixels[0] == RED32);
        if (d == 1)
        {
            CHECK(r.frames[1].empty()); // the gap frame keeps its slot
        }
        else
        {
            CHECK(r.frames[1].pixels[0] == GREEN32);
        }
    }

    uint8_t group = 5;
    CHECK(loader.replaceUopGroup(body, group));
    CHECK(group == 7);
    CHECK(cache.getAnimationFrames(body, 5, 2).frames.size() == 2);

    auto bwtFrames = cache.getAnimationFrames(body, 9, 4);
    REQUIRE(bwtFrames.frames.size() == 1);
    CHECK(bwtFrames.frames[0].width == 2);
    CHECK(bwtFrames.frames[0].pixels[1] == GREEN32);

    CHECK(cache.getAnimationFrames(body, 8, 0).frames.empty());
}

TEST_CASE("world directions fold onto the five stored ones")
{
    struct Case
    {
        uint8_t in, out;
        bool mirror;
    };
    const Case cases[] = {{0, 3, true}, {1, 2, true}, {2, 1, true}, {3, 0, false},
                          {4, 1, false}, {5, 2, false}, {6, 3, false}, {7, 4, false}};
    for (const auto& c : cases)
    {
        uint8_t d   = c.in;
        bool mirror = false;
        AnimationCache::getAnimDirection(d, mirror);
        CHECK(d == c.out);
        CHECK(mirror == c.mirror);
    }

    // 3 and 7 leave the mirror flag as it was.
    uint8_t d   = 3;
    bool mirror = true;
    AnimationCache::getAnimDirection(d, mirror);
    CHECK(mirror);
}

namespace
{

// A human (400), a horse (0xCC, animal), a monster (3) and a shirt (AnimID 0x200 = 512).
struct WorldFixture
{
    TempDir dir;
    AnimationsLoader loader;
    std::unique_ptr<AnimationCache> cache;
    std::unique_ptr<MobileAnimation> actions;

    WorldFixture()
    {
        MulAnimFile anim;
        for (int action : {0, 1, 2, 3, 4, 7, 15, 25})
        {
            for (int d = 0; d < MAX_DIRECTIONS; ++d)
            {
                anim.set(MulAnimFile::peopleBlock(400, action, d), {Sprite::solid(10, 20, 1, 5, -4), Sprite::solid(10, 20, 1, 5, -4)});
                anim.set(MulAnimFile::peopleBlock(512, action, d), {Sprite::solid(6, 8, 3, 3, -12), Sprite::solid(6, 8, 3, 3, -12)});
            }
        }
        for (int d = 0; d < MAX_DIRECTIONS; ++d)
        {
            anim.set(MulAnimFile::lowBlock(0xCC, 2, d), {Sprite::solid(30, 16, 2, 15, 0)});
            anim.set(MulAnimFile::highBlock(3, 1, d), {Sprite::solid(8, 8, 2)});
            anim.set(MulAnimFile::highBlock(3, 4, d), {Sprite::solid(8, 8, 2)});
        }
        anim.write(dir);
        REQUIRE(loader.load(configFor(dir)));
        cache   = std::make_unique<AnimationCache>(loader);
        actions = std::make_unique<MobileAnimation>(*cache);
        actions->setRandom([] { return 0u; });
    }
};

} // namespace

TEST_CASE("action selection for people follows ClassicUO")
{
    WorldFixture w;
    auto& a = *w.actions;

    MobileAnimState human;
    human.graphic = 400;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::Stand));

    human.isWalking = true;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::WalkUnarmed));
    human.isRunning = true;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::RunUnarmed));

    // A two-handed weapon outside the 0x240..0x3E1 range switches to the armed cycles.
    human.twoHanded = {true, 0x0100, false};
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::RunArmed));
    human.isRunning = false;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::WalkArmed));

    // War mode, walking: the war walk.
    human.twoHanded = {};
    human.inWarMode = true;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::WalkWarmode));

    // War mode, standing, empty hands: the one-handed attack stance.
    human.isWalking = false;
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::StandOnehandedAttack));
    // A two-handed staff from the base table: the two-handed stance.
    human.twoHanded = {true, 0x0263, false};
    CHECK(a.groupForAnimation(human) == uint8_t(PeopleAnimationGroup::StandTwohandedAttack));

    MobileAnimState rider;
    rider.graphic   = 400;
    rider.isMounted = true;
    CHECK(a.groupForAnimation(rider) == uint8_t(PeopleAnimationGroup::OnmountStand));
    rider.isWalking = true;
    CHECK(a.groupForAnimation(rider) == uint8_t(PeopleAnimationGroup::OnmountRideSlow));
    rider.isRunning = true;
    CHECK(a.groupForAnimation(rider) == uint8_t(PeopleAnimationGroup::OnmountRideFast));

    // The mount itself: an animal standing still plays its Stand group.
    rider.isWalking = false;
    rider.isRunning = false;
    CHECK(a.groupForAnimation(rider, 0xCC) == uint8_t(LowAnimationGroup::Stand));

    // A server action on a people body with no mobtypes flags passes through unchanged.
    MobileAnimState acting;
    acting.graphic             = 400;
    acting.animationFromServer = true;
    acting.animationGroup      = 9;
    CHECK(a.groupForAnimation(acting) == 9);
}

TEST_CASE("action selection for monsters and server animations")
{
    WorldFixture w;
    auto& a = *w.actions;

    MobileAnimState monster;
    monster.graphic = 3;
    CHECK(a.groupForAnimation(monster) == uint8_t(HighAnimationGroup::Stand));
    monster.isWalking = true;
    CHECK(a.groupForAnimation(monster) == uint8_t(HighAnimationGroup::Walk));

    // A server action the monster has no frames for falls back to Stand (1).
    monster.isWalking           = false;
    monster.animationFromServer = true;
    monster.animationGroup      = 6;
    CHECK(a.groupForAnimation(monster) == 1);
    monster.animationGroup = 4;
    CHECK(a.groupForAnimation(monster) == 4);

    // 0xE2 attack: without a mobtypes entry every body is handled as a monster.
    MobileAnimState human;
    human.graphic = 400;
    CHECK(a.objectNewAnimation(human, 0, 0, 1) == 5);
    CHECK(a.objectNewAnimation(human, 0, 0, 2) == 6);
    CHECK(a.objectNewAnimation(human, 3, 0, 1) == 2); // die
    CHECK(a.objectNewAnimation(human, 5, 0, 0) == 17); // fidget
    CHECK(a.objectNewAnimation(human, 7, 1, 0) == 33); // emote salute
    CHECK(a.objectNewAnimation(human, 12, 0, 0) == 0);

    CHECK(w.loader.getDeathAction(400, 0, AnimationGroupsType::Human, false) == uint8_t(PeopleAnimationGroup::Die1));
    CHECK(w.loader.getDeathAction(0xCC, 0, AnimationGroupsType::Animal, true) == uint8_t(LowAnimationGroup::Die2));
    CHECK(w.loader.getDeathAction(3, 0, AnimationGroupsType::SeaMonster, false) == 8);
}

TEST_CASE("paperdoll order: base table, reorder rules and cloak by facing")
{
    MobileEquipment eq;
    eq.at(Layer::Shirt)  = {true, 0x1517, 0x0190};
    eq.at(Layer::Pants)  = {true, 0x152E, 0x01EB};
    eq.at(Layer::Shoes)  = {true, 0x170B, 0x0171};
    eq.at(Layer::Cloak)  = {true, 0x1515, 0x0199};
    eq.at(Layer::Helmet) = {true, 0x1408, 0x01C0};

    std::array<Layer, PaperdollOrder::N> order{};
    const int n = PaperdollOrder::buildInWorld(eq, false, 4, order);
    const std::vector<Layer> got(order.begin(), order.begin() + n);

    auto pos = [&](Layer l) { return std::find(got.begin(), got.end(), l) - got.begin(); };

    CHECK(n == 23); // 25 minus the sentinel and the backpack
    // Pants 0x1EB are forced after the shoes.
    CHECK(pos(Layer::Shoes) < pos(Layer::Pants));
    // Facing south-west (4): cloak goes right below the helmet.
    CHECK(pos(Layer::Cloak) + 1 == pos(Layer::Helmet));

    std::array<Layer, PaperdollOrder::N> north{};
    const int nn = PaperdollOrder::buildInWorld(eq, false, 0, north);
    CHECK(north[nn - 1] == Layer::Cloak);

    std::array<Layer, PaperdollOrder::N> south{};
    PaperdollOrder::buildInWorld(eq, false, 3, south);
    CHECK(south[0] == Layer::Cloak);

    // Bone arms (AnimID 0x210) select the arms-late table.
    std::array<uint16_t, PaperdollOrder::N> gfx{};
    gfx[size_t(Layer::Arms)] = 0x210;
    std::array<Layer, PaperdollOrder::N> raw{};
    PaperdollOrder::build(gfx, false, raw);
    const auto armsAt  = std::find(raw.begin(), raw.end(), Layer::Arms) - raw.begin();
    const auto torsoAt = std::find(raw.begin(), raw.end(), Layer::Torso) - raw.begin();
    CHECK(torsoAt < armsAt);
}

TEST_CASE("covered layers, gargoyle equipment, mounts and chairs")
{
    MobileEquipment eq;
    eq.at(Layer::Robe)  = {true, 0x1F03, 0x01F5};
    eq.at(Layer::Torso) = {true, 0x13CC, 0x0200};
    eq.at(Layer::Arms)  = {true, 0x13CD, 0x0201};
    eq.at(Layer::Pants) = {true, 0x152E, 0x01EB};
    eq.at(Layer::Legs)  = {true, 0x13CB, 0x0202};

    CHECK(isCovered(eq, 400, Layer::Torso));
    CHECK(isCovered(eq, 400, Layer::Arms));
    CHECK(isCovered(eq, 400, Layer::Pants)); // legs armour hides pants
    CHECK_FALSE(isCovered(eq, 400, Layer::Robe));

    eq.at(Layer::Robe).graphic = 0x9985; // open robe
    CHECK_FALSE(isCovered(eq, 400, Layer::Torso));

    MobileEquipment hood;
    hood.at(Layer::Robe) = {true, 0x2683, 0x0300};
    hood.at(Layer::Hair) = {true, 0x203B, 0x00C4};
    CHECK(isCovered(hood, 400, Layer::Hair));
    CHECK(isCovered(hood, 0x029A, Layer::Hair)); // the classic hooded robes cover every body
    hood.at(Layer::Robe).graphic = 0xA0AB;
    CHECK(isCovered(hood, 400, Layer::Hair));
    CHECK_FALSE(isCovered(hood, 0x029A, Layer::Hair)); // the newer hoods leave gargoyle horns out

    CHECK(fixGargoyleEquipment(0x01D5) == 0x0156);
    CHECK(fixGargoyleEquipment(0x1234) == 0x1234);

    auto horse = findMount(0x3EA2);
    REQUIRE(horse);
    CHECK(horse->graphic == 0x00CC);
    CHECK(findMount(0x3EB4)->offsetY == -9);
    CHECK(mountGraphicForAnimation(0x3E9B, 0) == 0x00C0);
    CHECK(mountGraphicForAnimation(0x3EA2, 0) == 0x00CC);
    CHECK(mountGraphicForAnimation(0x3EA2, 0x0123) == 0x0123);

    auto chair = findChair(0x0459);
    REQUIRE(chair);
    CHECK(chair->direction1 == 0);
    CHECK(chair->direction2 == -1);

    // Like ClassicUO's TryParse, an unparsable graphic ("0x10") reads as 0 and still counts.
    CHECK(loadChairTable("# custom\n0x10,1,2,3,4,5,6\n4660, 0, 2, 4, 6, -4, -4 ; stool\n") == 2);
    CHECK(findChair(4660)->offsetY == -4);
    CHECK(findChair(0)->direction4 == 4);
    CHECK_FALSE(findChair(0x0459));
    resetChairTable();
    CHECK(findChair(0x0459));

    // A north-south seat (d1 = 0, d3 = 4) turns an east-facing sitter to the south.
    AnimationsLoader loader;
    uint8_t dir = 2;
    bool mirror = false;
    int x = 0, y = 0;
    loader.fixSittingDirection(dir, mirror, x, y, *findChair(0x0459));
    CHECK(dir == 1);
    CHECK_FALSE(mirror);
}

TEST_CASE("mobile draw list layers shadows, mount, rider and equipment")
{
    WorldFixture w;
    MobileRenderer renderer(*w.cache, *w.actions);

    MobileDrawInput in;
    in.body      = 400;
    in.hue       = 0x8021; // partial-hue bit on the body is stripped
    in.direction = 4;      // south-west: stored direction 1, not mirrored
    in.anim.graphic   = 400;
    in.anim.isMounted = true;
    in.equipment.at(Layer::Mount) = {true, 0x3EA2, 0, 0x0455};
    in.equipment.at(Layer::Shirt) = {true, 0x1517, 512, 0x0021, true};

    bool mirror = false;
    auto list   = renderer.build(in, mirror);

    using K = MobileDrawCommand::Kind;
    REQUIRE(list.commands.size() == 5);
    CHECK(list.commands[0].kind == K::Shadow);
    CHECK(list.commands[1].kind == K::Shadow);
    CHECK(list.commands[2].kind == K::Mount);
    CHECK(list.commands[3].kind == K::Body);
    CHECK(list.commands[4].kind == K::Equipment);
    CHECK(list.commands[4].layer == Layer::Shirt);

    const auto& mount = list.commands[2];
    CHECK(mount.graphic == 0xCC);
    CHECK(mount.action == uint8_t(LowAnimationGroup::Stand));
    CHECK(mount.direction == 1);
    CHECK_FALSE(mount.mirror);
    CHECK(mount.hue == 0x0455);
    CHECK_FALSE(mount.partialHue);
    // drawX = 22, drawY = 19; the frame hangs from its centre.
    CHECK(mount.x == 22 - 15);
    CHECK(mount.y == 19 - (16 + 0));

    const auto& body = list.commands[3];
    CHECK(body.action == uint8_t(PeopleAnimationGroup::OnmountStand));
    CHECK(body.hue == 0x0021);
    CHECK(body.partialHue);
    CHECK(body.x == 22 - 5);
    CHECK(body.y == 19 - (20 - 4));

    const auto& shirt = list.commands[4];
    CHECK(shirt.graphic == 512);
    CHECK(shirt.hue == 0x0021);
    CHECK(shirt.partialHue);

    // The body shadow sits 10px lower than the body.
    CHECK(list.commands[0].y == body.y + 10);

    CHECK(renderer.hitTest(list, body.x + 1, body.y + 1));
    CHECK_FALSE(renderer.hitTest(list, -100, -100));

    // Facing north-east (1) mirrors direction 2.
    in.direction = 1;
    list         = renderer.build(in, mirror);
    CHECK(mirror);
    CHECK(list.commands[3].direction == 2);
    CHECK(list.commands[3].x == 22 - (10 - 5));

    // A dead mobile casts no shadow and loses nothing else here.
    in.isDead = true;
    list      = renderer.build(in, mirror);
    CHECK(std::none_of(list.commands.begin(), list.commands.end(),
                       [](const MobileDrawCommand& c) { return c.kind == K::Shadow; }));
}

TEST_CASE("animation clock loops client actions and ends server actions")
{
    MobileAnimState state;
    MobileAnimClock clock;

    uint32_t now = 1000;
    CHECK(clock.tick(state, now, 3, false) == MobileAnimClock::Result::Advanced);
    CHECK(clock.animIndex == 1);
    CHECK(clock.tick(state, now + 10, 3, false) == MobileAnimClock::Result::Waiting); // 80 ms not elapsed
    now += 81;
    CHECK(clock.tick(state, now, 3, false) == MobileAnimClock::Result::Advanced);
    now += 81;
    CHECK(clock.tick(state, now, 3, false) == MobileAnimClock::Result::LoopedToStart);
    CHECK(clock.animIndex == 0);
    now += 81;
    CHECK(clock.tick(state, now, 3, true) == MobileAnimClock::Result::Waiting);

    // Packet 0x6E: action 9, 3 frames, forward, played once, interval 0 (160 ms per frame).
    clock.setAnimation(state, 9, now, 0, 3, 1, false, true, true);
    CHECK(state.animationGroup == 9);
    CHECK(state.animationFromServer);
    for (int i = 0; i < 2; ++i)
    {
        now += 161;
        clock.tick(state, now, 3, false);
    }
    CHECK(clock.animIndex == 2);
    now += 161;
    clock.tick(state, now, 3, false);
    CHECK(state.animationGroup == 0xFF);
    CHECK_FALSE(state.animationFromServer);
    CHECK(clock.animIndex == 0);
}
