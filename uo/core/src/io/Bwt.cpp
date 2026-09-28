// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Utility/BwtDecompress.cs).
#include "uo/io/Bwt.h"

#include <array>
#include <cstring>

namespace uo::io
{

namespace
{

constexpr int64_t kMaxOutput = 256ll * 1024 * 1024;

void shiftLeft(std::array<uint8_t, 257>& symbols, int max)
{
    for (int i = 0; i < max; ++i)
    {
        symbols[i] = symbols[i + 1];
    }
}

// Symbols ordered by descending count; ties keep the lower symbol first.
std::array<uint8_t, 256> frequencyOrder(const std::array<int32_t, 768>& counts)
{
    std::array<int32_t, 256> tmp{};
    std::memcpy(tmp.data(), counts.data(), sizeof(tmp));

    std::array<uint8_t, 256> out{};
    for (int i = 0; i < 256; ++i)
    {
        uint32_t value = 0;
        uint8_t index  = 0;
        for (int j = 0; j < 256; ++j)
        {
            if (static_cast<uint32_t>(tmp[j]) > value)
            {
                index = static_cast<uint8_t>(j);
                value = static_cast<uint32_t>(tmp[j]);
            }
        }
        if (value == 0)
        {
            break;
        }
        out[i]     = index;
        tmp[index] = 0;
    }
    return out;
}

std::vector<uint8_t> inverseTransform(std::span<const uint8_t> input)
{
    if (input.size() < 1024)
    {
        return {};
    }

    std::array<uint8_t, 257> symbols{};
    for (int i = 0; i < 256; ++i)
    {
        symbols[i] = static_cast<uint8_t>(i);
    }

    // [0,256): per-symbol counts, [256,512): cursor, [512,768): end.
    std::array<int32_t, 768> partial{};
    std::memcpy(partial.data(), input.data(), 1024);

    int64_t sum = 0;
    int nonZero = 0;
    for (int i = 0; i < 256; ++i)
    {
        sum += partial[i];
        if (partial[i] != 0)
        {
            ++nonZero;
        }
    }

    if (sum <= 0 || sum > kMaxOutput)
    {
        return {};
    }

    const auto len  = static_cast<size_t>(sum);
    const auto freq = frequencyOrder(partial);

    auto at = [&](int64_t idx) -> int {
        return idx >= 0 && static_cast<size_t>(idx) < input.size() ? input[static_cast<size_t>(idx)] : -1;
    };

    int64_t m = 0;
    for (int i = 0; i < nonZero; ++i)
    {
        const uint8_t f = freq[i];
        const int sym   = at(m + 1024);
        if (sym < 0)
        {
            return {};
        }
        symbols[sym]      = f;
        partial[f + 256]  = static_cast<int32_t>(m + 1);
        m                += partial[f];
        partial[f + 512]  = static_cast<int32_t>(m);
    }

    std::vector<uint8_t> output(len);
    uint8_t val = symbols[0];

    for (size_t count = 0; count < len; ++count)
    {
        int32_t& cursor = partial[val + 256];
        output[count]   = val;

        if (cursor >= partial[val + 512])
        {
            if (nonZero-- > 0)
            {
                shiftLeft(symbols, nonZero);
                val = symbols[0];
            }
        }
        else
        {
            const int idx = at(int64_t(cursor) + 1024);
            if (idx < 0)
            {
                return {};
            }
            ++cursor;

            if (idx != 0)
            {
                shiftLeft(symbols, idx);
                symbols[idx] = val;
                val          = symbols[0];
            }
        }
    }

    return output;
}

} // namespace

std::vector<uint8_t> bwtDecompress(std::span<const uint8_t> buffer)
{
    // u32 header, then a move-to-front coded stream. ClassicUO builds a sorted 64K
    // table for the MTF, which is the identity permutation; only its first 256 entries
    // are ever touched.
    if (buffer.size() < 5)
    {
        return {};
    }

    std::array<uint8_t, 256> mtf{};
    for (int i = 0; i < 256; ++i)
    {
        mtf[i] = static_cast<uint8_t>(i);
    }

    // As in ClassicUO the list is one byte shorter than the stream it decodes (the last
    // byte read is never emitted) and keeps a trailing zero.
    std::vector<uint8_t> list(buffer.size() - 4, 0);
    size_t out = 0;

    size_t pos        = 4;
    uint8_t firstChar = buffer[pos++];

    while (pos < buffer.size())
    {
        uint8_t current     = firstChar;
        const uint8_t value = mtf[current];
        for (; current > 0; --current)
        {
            mtf[current] = mtf[current - 1];
        }
        mtf[0]      = value;
        list[out++] = value;
        firstChar   = buffer[pos++];
    }

    return inverseTransform(list);
}

} // namespace uo::io
