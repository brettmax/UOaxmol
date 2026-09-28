// SPDX-License-Identifier: BSD-2-Clause
// Builds a tiny, fully synthetic UO client folder (no copyrighted data) for the converter tests.
#pragma once

#include "TestUtil.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

}  // namespace fixture
