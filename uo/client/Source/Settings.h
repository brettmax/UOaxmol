// SPDX-License-Identifier: MIT
#pragma once

#include "uo/io/ClientVersion.h"

#include <cstdint>
#include <string>

// Client configuration, read from settings.json in the writable path (so edits survive
// reinstalls) or, failing that, the one shipped in Content/. Mirrors the relevant subset of
// ClassicUO's settings.json.
struct Settings
{
    std::string uoDirectory;
    // uoconvert's output folder (its --out), if assets were converted. Optional.
    std::string assetsDirectory;
    uo::ClientVersion clientVersion = uo::makeVersion(7, 0, 15, 1);
    std::string host                = "127.0.0.1";
    std::uint16_t port              = 2593;
    std::string account;
    std::string password;
    // Write the password back (obfuscated, ClassicUO-style) so autoLogin keeps working after
    // the first launch. Off means it is typed each time.
    bool savePassword = true;
    bool ignoreRelayAddress = true;
    int map                 = 0;
    // ClassicUO's AutoLogin: connect on start and play the first character.
    bool autoLogin = false;

    static Settings load();
    void save() const;
};
