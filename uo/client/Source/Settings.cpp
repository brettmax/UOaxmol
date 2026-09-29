// SPDX-License-Identifier: MIT
#include "Settings.h"

#include "axmol/axmol.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

using namespace ax;

namespace
{
std::string userPath()
{
    return FileUtils::getInstance()->getWritablePath() + "settings.json";
}

// Saved passwords are obfuscated the way ClassicUO's Crypter does it: a "1-" marker, then each
// byte XORed with a rolling key and written as hex. This only keeps the password from being
// readable at a glance in settings.json; it is not encryption. A value without the marker is
// taken as typed by hand and used as is.
constexpr std::string_view kObfuscatedPrefix = "1-";

std::uint8_t obfuscationKey(std::size_t i)
{
    return static_cast<std::uint8_t>(0x5A ^ (i * 31 + 7));
}

std::string obfuscate(std::string_view plain)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out(kObfuscatedPrefix);
    for (std::size_t i = 0; i < plain.size(); ++i)
    {
        const auto b = static_cast<std::uint8_t>(static_cast<std::uint8_t>(plain[i]) ^ obfuscationKey(i));
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

std::string deobfuscate(std::string_view stored)
{
    if (!stored.starts_with(kObfuscatedPrefix))
        return std::string(stored);

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    const std::string_view hex = stored.substr(kObfuscatedPrefix.size());
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return {};
        out.push_back(static_cast<char>((hi << 4 | lo) ^ obfuscationKey(i / 2)));
    }
    return out;
}
}  // namespace

Settings Settings::load()
{
    Settings s;
    auto* fu         = FileUtils::getInstance();
    std::string text = fu->isFileExist(userPath()) ? fu->getStringFromFile(userPath()) : fu->getStringFromFile("settings.json");

    // Notepad and Windows PowerShell save UTF-8 with a byte order mark, which rapidjson rejects.
    if (text.starts_with("\xEF\xBB\xBF"))
        text.erase(0, 3);

    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError() || !doc.IsObject())
    {
        AXLOGW("settings.json is not valid JSON; using defaults");
        return s;
    }

    auto str = [&](const char* key, std::string& out) {
        if (doc.HasMember(key) && doc[key].IsString())
            out = doc[key].GetString();
    };
    str("uoDirectory", s.uoDirectory);
    str("assetsDirectory", s.assetsDirectory);
    str("host", s.host);
    str("account", s.account);
    str("password", s.password);
    s.password = deobfuscate(s.password);
    if (doc.HasMember("savePassword") && doc["savePassword"].IsBool())
        s.savePassword = doc["savePassword"].GetBool();

    if (doc.HasMember("clientVersion") && doc["clientVersion"].IsString())
    {
        if (auto v = uo::parseClientVersion(doc["clientVersion"].GetString()))
            s.clientVersion = *v;
    }
    if (doc.HasMember("port") && doc["port"].IsInt())
        s.port = static_cast<std::uint16_t>(doc["port"].GetInt());
    if (doc.HasMember("ignoreRelayAddress") && doc["ignoreRelayAddress"].IsBool())
        s.ignoreRelayAddress = doc["ignoreRelayAddress"].GetBool();
    if (doc.HasMember("autoLogin") && doc["autoLogin"].IsBool())
        s.autoLogin = doc["autoLogin"].GetBool();
    if (doc.HasMember("map") && doc["map"].IsInt())
        s.map = doc["map"].GetInt();
    return s;
}

void Settings::save() const
{
    rapidjson::Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();
    doc.AddMember("uoDirectory", rapidjson::Value(uoDirectory.c_str(), a), a);
    doc.AddMember("assetsDirectory", rapidjson::Value(assetsDirectory.c_str(), a), a);
    doc.AddMember("clientVersion", rapidjson::Value(uo::clientVersionToString(clientVersion).c_str(), a), a);
    doc.AddMember("host", rapidjson::Value(host.c_str(), a), a);
    doc.AddMember("port", port, a);
    doc.AddMember("account", rapidjson::Value(account.c_str(), a), a);
    // Only kept when asked for, and then obfuscated; autoLogin needs it on later launches.
    doc.AddMember("savePassword", savePassword, a);
    if (savePassword && !password.empty())
        doc.AddMember("password", rapidjson::Value(obfuscate(password).c_str(), a), a);
    doc.AddMember("ignoreRelayAddress", ignoreRelayAddress, a);
    doc.AddMember("map", map, a);
    doc.AddMember("autoLogin", autoLogin, a);

    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buf);
    doc.Accept(w);
    FileUtils::getInstance()->writeStringToFile(buf.GetString(), userPath());
}
