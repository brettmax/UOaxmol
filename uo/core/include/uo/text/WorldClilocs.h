// SPDX-License-Identifier: BSD-2-Clause
// Cliloc lookup for the uo::world packet handlers, over assets::Cliloc.
#pragma once

// Available once uo::world is in uocore; until then this header is empty.
#if __has_include("uo/world/Events.h")

#include "uo/assets/Cliloc.h"
#include "uo/world/Events.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#define UO_TEXT_HAS_WORLD_CLILOCS 1

namespace uo::text
{

// Attach with World::setClilocs(&resolver). The cliloc table must outlive it.
//
// translate() follows ClassicUO's ClilocLoader.Translate, which never fails: an unknown number
// yields "MegaCliloc: missing <n> ..." with the arguments substituted, so the player still sees
// what the server sent. Capitalization is Unicode-aware (every word, as the original does).
class WorldClilocs final : public world::ClilocResolver
{
public:
    explicit WorldClilocs(const assets::Cliloc& clilocs) : _clilocs(&clilocs) {}

    std::string get(std::uint32_t cliloc) override
    {
        const std::string* s = _clilocs->get(static_cast<std::int32_t>(cliloc));
        return s ? *s : std::string{};
    }

    std::optional<std::string> translate(std::uint32_t cliloc, std::string_view args, bool capitalize) override
    {
        return _clilocs->translate(static_cast<std::int32_t>(cliloc), args, capitalize);
    }

private:
    const assets::Cliloc* _clilocs;
};

}  // namespace uo::text

#endif
