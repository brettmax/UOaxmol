// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Data/BuffTable.cs).
#pragma once

#include <cstdint>
#include <span>

namespace uo::world
{

// Buff icon index -> gump graphic. 0xDF icon ids index into this table.
std::span<const uint16_t> defaultBuffTable() noexcept;

} // namespace uo::world
