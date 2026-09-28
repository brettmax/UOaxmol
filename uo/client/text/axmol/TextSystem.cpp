// SPDX-License-Identifier: BSD-2-Clause

#include "axmol/TextSystem.h"

#include "uo/assets/Installation.h"
#include "uo/io/ClientVersion.h"

#include <cassert>

namespace uo::client::text
{

TextSystem& TextSystem::instance()
{
    static TextSystem system;
    return system;
}

bool TextSystem::init(const assets::Installation& installation)
{
    _installation = &installation;
    _hues         = std::make_unique<uo::text::HuesResolver>(installation.hues());
    _fonts.setHueResolver(_hues.get());
    _defaultFont = installation.options().version >= makeVersion(3, 0, 5, 'd') ? 1 : 0;  // CV_305D
    return _fonts.load(installation);
}

void TextSystem::shutdown()
{
    _fonts.setHueResolver(nullptr);
    _hues.reset();
    _installation = nullptr;
}

const assets::Cliloc& TextSystem::cliloc() const
{
    assert(_installation && "TextSystem::init has not run");
    return _installation->cliloc();
}

}  // namespace uo::client::text
