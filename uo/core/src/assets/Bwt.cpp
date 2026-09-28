// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (BwtDecompress.cs).

#include "uo/assets/Bwt.h"

#include <array>
#include <cstring>

namespace uo::assets
{

namespace
{

void shiftLeft(std::array<uint8_t, 256>& table, int max)
{
    for (int i = 0; i < max; ++i)
        table[i] = table[i + 1];
}

// Orders byte values by descending count; ties go to the lower byte value.
int frequency(const int* counts, std::array<uint8_t, 256>& output)
{
    int tmp[256];
    std::memcpy(tmp, counts, sizeof(tmp));
    int n = 0;

    for (int i = 0; i < 256; ++i)
    {
        uint32_t value = 0;
        uint8_t index  = 0;

        for (int j = 0; j < 256; ++j)
        {
            if (static_cast<uint32_t>(tmp[j]) > value && tmp[j] > 0)
            {
                index = static_cast<uint8_t>(j);
                value = static_cast<uint32_t>(tmp[j]);
            }
        }

        if (value == 0)
            break;

        output[i]  = index;
        tmp[index] = 0;
        ++n;
    }

    return n;
}

std::vector<uint8_t> internalDecompress(std::span<const uint8_t> input)
{
    if (input.size() < 1024)
        return {};

    std::array<uint8_t, 256> symbolTable{};
    std::array<uint8_t, 256> freq{};
    int partial[256 * 3] = {};

    for (int i = 0; i < 256; ++i)
        symbolTable[i] = static_cast<uint8_t>(i);

    for (int i = 0; i < 256; ++i)
    {
        const uint8_t* p = input.data() + i * 4;
        partial[i]       = static_cast<int>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                      (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
    }

    int64_t sum = 0;
    int nonZeroCount = 0;

    for (int i = 0; i < 256; ++i)
    {
        if (partial[i] < 0)
            return {};
        sum += partial[i];
        if (partial[i] != 0)
            ++nonZeroCount;
    }

    // The payload after the 1024-byte count header is at most one index per output byte;
    // anything claiming far more output than that is corrupt.
    if (sum <= 0 || sum > static_cast<int64_t>(input.size()) * 256)
        return {};

    const auto len = static_cast<size_t>(sum);
    std::vector<uint8_t> output(len);

    frequency(partial, freq);

    for (int i = 0, m = 0; i < nonZeroCount; ++i)
    {
        const uint8_t f = freq[i];

        if (static_cast<size_t>(m) + 1024 >= input.size())
            return {};

        symbolTable[input[m + 1024]] = f;
        partial[f + 256]             = m + 1;
        m += partial[f];
        partial[f + 512] = m;
    }

    uint8_t val  = symbolTable[0];
    size_t count = 0;

    do
    {
        int& firstVal = partial[val + 256];
        output[count] = val;

        if (firstVal >= partial[val + 512])
        {
            if (nonZeroCount-- > 0)
            {
                shiftLeft(symbolTable, nonZeroCount);
                val = symbolTable[0];
            }
        }
        else
        {
            if (static_cast<size_t>(firstVal) + 1024 >= input.size())
                return {};

            const uint8_t idx = input[firstVal + 1024];
            ++firstVal;

            if (idx != 0)
            {
                shiftLeft(symbolTable, idx);
                symbolTable[idx] = val;
                val              = symbolTable[0];
            }
        }

        ++count;
    } while (count < len);

    return output;
}

}  // namespace

std::vector<uint8_t> bwtDecompress(std::span<const uint8_t> buffer)
{
    if (buffer.size() < 5)
        return {};

    // Move-to-front decode over the byte alphabet. The original builds a sorted 64K
    // ushort table, which is the identity permutation; only its first 256 slots are
    // ever touched, so a 256-entry table is equivalent.
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; ++i)
        table[i] = static_cast<uint8_t>(i);

    // Matches the original exactly: one MTF output per byte from offset 4 up to but not
    // including the last byte, and a trailing zero in the (size - 4)-byte list.
    std::vector<uint8_t> list(buffer.size() - 4, 0);
    size_t pos        = 4;
    uint8_t firstChar = buffer[pos++];
    size_t i          = 0;

    while (pos < buffer.size())
    {
        uint8_t current     = firstChar;
        const uint8_t value = table[current];

        for (; current > 0; --current)
            table[current] = table[current - 1];

        table[0] = value;
        list[i++] = value;
        firstChar = buffer[pos++];
    }

    return internalDecompress(list);
}

}  // namespace uo::assets
