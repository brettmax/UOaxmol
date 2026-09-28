// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/WalkerManager.cs).

#pragma once

#include "uo/movement/MovementConstants.h"

#include <array>
#include <cstdint>

namespace uo::movement
{

// Fast-walk prevention keys the server hands out (0xBF subcommands 1 and 2). Each walk request
// consumes one key; zero means "no key" and is what an empty stack yields.
class FastWalkStack
{
public:
    // 0xBF/1 sends six keys but the stack holds five; out-of-range indices are dropped, as in ClassicUO.
    void set(int index, uint32_t value)
    {
        if (index >= 0 && index < kMaxFastWalkStackSize)
        {
            _keys[index] = value;
        }
    }

    void add(uint32_t value)
    {
        for (auto& key : _keys)
        {
            if (key == 0)
            {
                key = value;
                return;
            }
        }
    }

    uint32_t take()
    {
        for (auto& key : _keys)
        {
            if (key != 0)
            {
                const uint32_t value = key;
                key = 0;
                return value;
            }
        }
        return 0;
    }

    void clear() { _keys.fill(0); }

private:
    std::array<uint32_t, kMaxFastWalkStackSize> _keys{};
};

}  // namespace uo::movement
