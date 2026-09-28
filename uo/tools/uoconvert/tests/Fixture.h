// SPDX-License-Identifier: BSD-2-Clause
// Builds a tiny, fully synthetic UO client folder (no copyrighted data) for the converter tests.
#pragma once

#include "TestUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <tuple>

#include "uo/io/UOFile.h"
#include <string>
#include <vector>

namespace fixture
{

using uotest::Bytes;
using uotest::le16;
using uotest::le32;

// Encoders: the inverse of what uocore decodes, written from the format descriptions.

// Static art: { u32 flags; u16 w; u16 h; u16 rowOffsets[h]; rows of (skip, run, colours...) + (0, 0) }.
inline Bytes encodeStatic(int w, int h, const std::vector<std::uint16_t>& px)
{
    std::vector<std::uint16_t> words;
    std::vector<std::uint16_t> offsets;
    for (int y = 0; y < h; ++y)
    {
        offsets.push_back(static_cast<std::uint16_t>(words.size()));
        int x = 0, last = 0;
        while (x < w)
        {
            if (!px[y * w + x])
            {
                ++x;
                continue;
            }
            int start = x;
            while (x < w && px[y * w + x])
                ++x;
            words.push_back(static_cast<std::uint16_t>(start - last));
            words.push_back(static_cast<std::uint16_t>(x - start));
            for (int i = start; i < x; ++i)
                words.push_back(px[y * w + i]);
            last = x;
        }
        words.push_back(0);
        words.push_back(0);
    }
    Bytes b;
    le32(b, 0);
    le16(b, static_cast<std::uint16_t>(w));
    le16(b, static_cast<std::uint16_t>(h));
    for (auto o : offsets)
        le16(b, o);
    for (auto v : words)
        le16(b, v);
    return b;
}

// Gump rows: i32 row offsets (4-byte units from the table start), then (colour, run) pairs.
inline Bytes encodeGump(int w, int h, const std::vector<std::uint16_t>& px)
{
    std::vector<std::vector<std::pair<std::uint16_t, std::uint16_t>>> rows(h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w;)
        {
            int s = x;
            while (x < w && px[y * w + x] == px[y * w + s])
                ++x;
            rows[y].push_back({px[y * w + s], static_cast<std::uint16_t>(x - s)});
        }
    Bytes b;
    std::int32_t at = h;
    for (const auto& r : rows)
    {
        le32(b, static_cast<std::uint32_t>(at));
        at += static_cast<std::int32_t>(r.size());
    }
    for (const auto& r : rows)
        for (auto [c, n] : r)
        {
            le16(b, c);
            le16(b, n);
        }
    return b;
}

// A .mul + .idx pair from { index -> (bytes, extra) }.
struct Indexed
{
    std::map<std::uint32_t, std::pair<Bytes, std::int32_t>> entries;
    std::uint32_t count = 0;

    void add(std::uint32_t i, Bytes b, std::int32_t extra = -1)
    {
        entries[i] = {std::move(b), extra};
        count      = std::max(count, i + 1);
    }

    void write(const uotest::TempDir& dir, const std::string& mul, const std::string& idx) const
    {
        Bytes data, index;
        data.push_back(0);  // offset 0 would look like "missing" to some readers
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto it = entries.find(i);
            if (it == entries.end())
            {
                le32(index, 0xFFFFFFFFu);
                le32(index, 0);
                le32(index, 0xFFFFFFFFu);
                continue;
            }
            le32(index, static_cast<std::uint32_t>(data.size()));
            le32(index, static_cast<std::uint32_t>(it->second.first.size()));
            le32(index, static_cast<std::uint32_t>(it->second.second));
            data.insert(data.end(), it->second.first.begin(), it->second.first.end());
        }
        dir.write(mul, data);
        dir.write(idx, index);
    }
};

// A one-block UOP archive: { name -> (stored bytes, decompressed size, flag) }.
inline Bytes buildUop(const std::vector<std::tuple<std::string, Bytes, std::uint32_t, std::uint16_t>>& files)
{
    Bytes f;
    le32(f, 0x50594D);
    le32(f, 5);
    le32(f, 0xFD23EC43);
    uotest::le64(f, 40);
    le32(f, 100);
    le32(f, static_cast<std::uint32_t>(files.size()));
    while (f.size() < 40)
        f.push_back(0);
    std::size_t dataAt = 40 + 12 + 34 * files.size();
    le32(f, static_cast<std::uint32_t>(files.size()));
    uotest::le64(f, 0);
    for (const auto& [name, stored, dsize, flag] : files)
    {
        uotest::le64(f, dataAt);
        le32(f, 0);
        le32(f, static_cast<std::uint32_t>(stored.size()));
        le32(f, dsize);
        uotest::le64(f, uo::io::UopFile::hash(name));
        le32(f, 0);
        le16(f, flag);
        dataAt += stored.size();
    }
    for (const auto& file : files)
        f.insert(f.end(), std::get<1>(file).begin(), std::get<1>(file).end());
    return f;
}

inline std::uint16_t rgb15(int r, int g, int b)
{
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}

// Encoder for the BWT stage uo::assets::bwtDecompress inverts (copied from uo/tests/AnimTests.cpp): a move-to-position list code over the
// symbols ordered by next use, then a move-to-front pass.
inline Bytes bwtEncode(const Bytes& payload)
{
    std::array<std::int32_t, 256> counts{};
    for (std::uint8_t b : payload)
    {
        ++counts[b];
    }

    // Descending count, lower symbol first on ties (the decoder's order).
    std::vector<int> order;
    std::array<std::int32_t, 256> tmp = counts;
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

    std::array<std::vector<std::size_t>, 256> occ;
    for (std::size_t t = 0; t < payload.size(); ++t)
    {
        occ[payload[t]].push_back(t);
    }

    std::vector<int> list;
    for (std::size_t t = 0; t < payload.size(); ++t)
    {
        if (std::find(list.begin(), list.end(), payload[t]) == list.end())
        {
            list.push_back(payload[t]);
        }
    }

    std::array<std::vector<std::uint8_t>, 256> seg;
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        seg[list[i]].push_back(static_cast<std::uint8_t>(i));
    }

    std::array<std::size_t, 256> used{};
    auto nextOcc = [&](int s) { return occ[s][used[s]]; };

    for (std::size_t t = 0; t < payload.size(); ++t)
    {
        const int v = payload[t];
        list.erase(list.begin());
        ++used[v];
        if (used[v] < occ[v].size())
        {
            const std::size_t when = nextOcc(v);
            std::size_t idx        = 0;
            while (idx < list.size() && nextOcc(list[idx]) < when)
            {
                ++idx;
            }
            seg[v].push_back(static_cast<std::uint8_t>(idx));
            list.insert(list.begin() + static_cast<std::ptrdiff_t>(idx), v);
        }
    }

    Bytes transformed;
    for (std::int32_t c : counts)
    {
        le32(transformed, static_cast<std::uint32_t>(c));
    }
    for (int s : order)
    {
        transformed.insert(transformed.end(), seg[s].begin(), seg[s].end());
    }

    Bytes out{0, 0, 0, 0};
    std::array<std::uint8_t, 256> mtf{};
    for (int i = 0; i < 256; ++i)
    {
        mtf[i] = static_cast<std::uint8_t>(i);
    }
    for (std::uint8_t v : transformed)
    {
        const auto it  = std::find(mtf.begin(), mtf.end(), v);
        const auto pos = static_cast<std::uint8_t>(it - mtf.begin());
        out.push_back(pos);
        std::rotate(mtf.begin(), it, it + 1);
    }
    out.push_back(0); // read by the decoder but never emitted
    return out;
}

// A one-preset SoundFont 2 bank: a looped 441 Hz sine on preset 0, bank 0.
inline Bytes sineSoundFont()
{
    auto chunk = [](const char* id, const Bytes& body) {
        Bytes b(id, id + 4);
        le32(b, static_cast<std::uint32_t>(body.size()));
        b.insert(b.end(), body.begin(), body.end());
        if (body.size() & 1)
            b.push_back(0);
        return b;
    };
    auto list = [&](const char* type, const std::vector<Bytes>& chunks) {
        Bytes body(type, type + 4);
        for (const auto& c : chunks)
            body.insert(body.end(), c.begin(), c.end());
        return chunk("LIST", body);
    };
    auto name = [](Bytes& b, const char* n) {
        std::size_t len = std::strlen(n);
        for (std::size_t i = 0; i < 20; ++i)
            b.push_back(i < len ? static_cast<std::uint8_t>(n[i]) : 0);
    };

    constexpr std::uint32_t kLen = 2200;  // 22 periods of 100 samples at 44.1 kHz
    Bytes smpl;
    for (std::uint32_t i = 0; i < kLen + 46; ++i)
        le16(smpl, static_cast<std::uint16_t>(static_cast<std::int16_t>(
                       i < kLen ? 20000.0 * std::sin(2.0 * 3.14159265358979 * i / 100.0) : 0.0)));

    Bytes phdr, pbag, pmod(10, 0), pgen, inst, ibag, imod(10, 0), igen, shdr;
    for (auto [n, bag] : {std::pair{"sine", 0}, std::pair{"EOP", 1}})
    {
        name(phdr, n);
        le16(phdr, 0), le16(phdr, 0), le16(phdr, static_cast<std::uint16_t>(bag));
        le32(phdr, 0), le32(phdr, 0), le32(phdr, 0);
    }
    le16(pbag, 0), le16(pbag, 0), le16(pbag, 1), le16(pbag, 0);
    le16(pgen, 41), le16(pgen, 0), le16(pgen, 0), le16(pgen, 0);  // instrument 0
    name(inst, "sine"), le16(inst, 0), name(inst, "EOI"), le16(inst, 1);
    le16(ibag, 0), le16(ibag, 0), le16(ibag, 2), le16(ibag, 0);
    le16(igen, 54), le16(igen, 1);  // sampleModes: loop
    le16(igen, 53), le16(igen, 0);  // sampleID 0
    le16(igen, 0), le16(igen, 0);
    name(shdr, "sine");
    le32(shdr, 0), le32(shdr, kLen), le32(shdr, 0), le32(shdr, kLen), le32(shdr, 44100);
    shdr.push_back(69), shdr.push_back(0), le16(shdr, 0), le16(shdr, 1);
    name(shdr, "EOS");
    for (int i = 0; i < 26; ++i)
        shdr.push_back(0);

    Bytes body{'s', 'f', 'b', 'k'};
    for (const Bytes& l : {list("sdta", {chunk("smpl", smpl)}),
                          list("pdta", {chunk("phdr", phdr), chunk("pbag", pbag), chunk("pmod", pmod),
                                        chunk("pgen", pgen), chunk("inst", inst), chunk("ibag", ibag),
                                        chunk("imod", imod), chunk("igen", igen), chunk("shdr", shdr)})})
        body.insert(body.end(), l.begin(), l.end());
    return chunk("RIFF", body);
}

// A format-0 MIDI file: one note (A4) held for `ticks` at 96 ticks per quarter, 120 bpm.
inline Bytes oneNoteMidi(std::uint8_t ticks)
{
    Bytes track{0x00, 0xC0, 0x00, 0x00, 0x90, 0x45, 0x64, ticks, 0x80, 0x45, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    Bytes b{'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96, 'M', 'T', 'r', 'k', 0, 0, 0,
            static_cast<std::uint8_t>(track.size())};
    b.insert(b.end(), track.begin(), track.end());
    return b;
}

}  // namespace fixture
