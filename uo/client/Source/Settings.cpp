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
}  // namespace

Settings Settings::load()
{
    Settings s;
    auto* fu         = FileUtils::getInstance();
    std::string text = fu->isFileExist(userPath()) ? fu->getStringFromFile(userPath()) : fu->getStringFromFile("settings.json");

    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError() || !doc.IsObject())
        return s;

    auto str = [&](const char* key, std::string& out) {
        if (doc.HasMember(key) && doc[key].IsString())
            out = doc[key].GetString();
    };
    str("uoDirectory", s.uoDirectory);
    str("host", s.host);
    str("account", s.account);
    str("password", s.password);

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
    doc.AddMember("clientVersion", rapidjson::Value(uo::clientVersionToString(clientVersion).c_str(), a), a);
    doc.AddMember("host", rapidjson::Value(host.c_str(), a), a);
    doc.AddMember("port", port, a);
    doc.AddMember("account", rapidjson::Value(account.c_str(), a), a);
    // The password is deliberately not written back to disk.
    doc.AddMember("ignoreRelayAddress", ignoreRelayAddress, a);
    doc.AddMember("map", map, a);
    doc.AddMember("autoLogin", autoLogin, a);

    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buf);
    doc.Accept(w);
    FileUtils::getInstance()->writeStringToFile(buf.GetString(), userPath());
}
