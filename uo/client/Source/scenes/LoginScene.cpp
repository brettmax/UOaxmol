// SPDX-License-Identifier: MIT
#include "LoginScene.h"

#include "GameClient.h"
#include "WorldScene.h"

#include <cstdlib>

using namespace ax;

namespace
{
constexpr const char* kFont = "fonts/arial.ttf";
const Color32 kText{230, 220, 200, 255};
const Color32 kMuted{150, 140, 120, 255};
}  // namespace

bool LoginScene::init()
{
    if (!Scene::init())
        return false;

    auto size   = _director->getVisibleSize();
    auto origin = _director->getVisibleOrigin();
    auto& cfg   = GameClient::instance().settings();

    auto bg = LayerColor::create(Color32(18, 14, 10, 255));
    addChild(bg, -1);

    auto title = Label::createWithTTF("AxmolUO", kFont, 42);
    title->setTextColor(kText);
    title->setPosition(origin + Vec2(size.width / 2, size.height - 80));
    addChild(title);

    float y   = size.height - 170;
    _dir      = addField("UO data folder", cfg.uoDirectory, y);
    _host     = addField("Server", cfg.host, y -= 50);
    _port     = addField("Port", std::to_string(cfg.port), y -= 50);
    _account  = addField("Account", cfg.account, y -= 50);
    _password = addField("Password", cfg.password, y -= 50, true);

    auto loginLabel = Label::createWithTTF("Log in", kFont, 26);
    loginLabel->setTextColor(kText);
    auto loginItem = MenuItemLabel::create(loginLabel, [this](Object*) { login(); });
    auto menu      = Menu::create(loginItem, nullptr);
    menu->setPosition(origin + Vec2(size.width / 2, y - 60));
    addChild(menu);

    _status = Label::createWithTTF("", kFont, 18);
    _status->setPosition(origin + Vec2(size.width / 2, y - 110));
    _status->setDimensions(size.width - 80, 0);
    _status->setHorizontalAlignment(TextHAlignment::CENTER);
    addChild(_status);

    _choices = Menu::create();
    _choices->setPosition(origin + Vec2(size.width / 2, y - 160));
    addChild(_choices);

    scheduleUpdate();
    return true;
}

ui::InputField* LoginScene::addField(const char* label, std::string_view value, float y, bool password)
{
    auto x = _director->getVisibleOrigin().x + _director->getVisibleSize().width / 2;

    auto l = Label::createWithTTF(label, kFont, 20);
    l->setTextColor(kMuted);
    l->setAnchorPoint(Vec2(1, 0.5f));
    l->setPosition(Vec2(x - 20, y));
    addChild(l);

    auto field = ui::InputField::create(label, kFont, 20);
    field->setTextColor(kText);
    field->setPlaceholderColor(Color32(90, 85, 75, 255));
    field->setAnchorPoint(Vec2(0, 0.5f));
    field->setPosition(Vec2(x, y));
    // Auto size measures the placeholder and wraps the value into that width, which cut
    // "127.0.0.1" to "127.0." and "2593" to "259"; give every field room for a data path.
    field->setAutoSize(false);
    field->setContentSize(Vec2(480, 28));
    if (password)
        field->setPasswordEnabled(true);
    field->setString(value);
    addChild(field);
    return field;
}

void LoginScene::onEnter()
{
    Scene::onEnter();
    auto& gc = GameClient::instance();

    gc.errorHandler = [this](const std::string& msg) { setStatus(msg, true); };
    gc.shardsHandler = [this](const std::vector<uo::net::ShardInfo>& shards) {
        if (shards.size() == 1)
        {
            // One shard, the ModernUO default: skip the picker like ClassicUO's auto-login.
            GameClient::instance().session().selectShard(shards[0].index);
            setStatus("Connecting to " + shards[0].name + "...");
            return;
        }
        std::vector<std::pair<std::string, std::function<void()>>> items;
        for (const auto& s : shards)
        {
            std::uint16_t index = s.index;
            items.emplace_back(s.name, [this, index] {
                GameClient::instance().session().selectShard(index);
                showChoices({});
                setStatus("Connecting...");
            });
        }
        setStatus("Choose a shard");
        showChoices(items);
    };
    gc.charactersHandler = [this](const std::vector<uo::net::CharacterSlot>& chars) {
        if (chars.empty())
        {
            setStatus("This account has no characters. Character creation is not ported yet.", true);
            return;
        }
        if (GameClient::instance().settings().autoLogin)
        {
            GameClient::instance().session().selectCharacter(chars[0].slot);
            setStatus("Entering Britannia...");
            return;
        }
        std::vector<std::pair<std::string, std::function<void()>>> items;
        for (const auto& c : chars)
        {
            std::uint32_t slot = c.slot;
            items.emplace_back(c.name, [this, slot] {
                GameClient::instance().session().selectCharacter(slot);
                showChoices({});
                setStatus("Entering Britannia...");
            });
        }
        setStatus("Choose a character");
        showChoices(items);
    };
    gc.enteredWorldHandler = [this] {
        _director->replaceScene(TransitionFade::create(0.4f, utils::createInstance<WorldScene>()));
    };
    gc.disconnectedHandler = [this] { setStatus("Disconnected.", true); };

    if (gc.settings().autoLogin && !gc.autoLoginDone && !gc.settings().account.empty())
    {
        gc.autoLoginDone = true;  // once per launch, not after every disconnect
        scheduleOnce([this](float) { login(); }, 0.1f, "autologin");
    }
}

void LoginScene::onExit()
{
    auto& gc               = GameClient::instance();
    gc.errorHandler        = nullptr;
    gc.shardsHandler       = nullptr;
    gc.charactersHandler   = nullptr;
    gc.enteredWorldHandler = nullptr;
    gc.disconnectedHandler = nullptr;
    Scene::onExit();
}

void LoginScene::update(float dt)
{
    GameClient::instance().update(dt);
}

void LoginScene::setStatus(std::string_view text, bool error)
{
    _status->setString(text);
    _status->setTextColor(error ? Color32(230, 90, 70, 255) : kText);
}

void LoginScene::showChoices(const std::vector<std::pair<std::string, std::function<void()>>>& choices)
{
    _choices->removeAllChildren();
    for (const auto& [name, action] : choices)
    {
        auto label = Label::createWithTTF(name, kFont, 22);
        label->setTextColor(kText);
        auto cb = action;
        _choices->addChild(MenuItemLabel::create(label, [cb](Object*) { cb(); }));
    }
    _choices->alignItemsVerticallyWithPadding(8);
}

void LoginScene::login()
{
    auto& gc  = GameClient::instance();
    auto& cfg = gc.settings();
    cfg.uoDirectory = std::string(_dir->getString());
    cfg.host        = std::string(_host->getString());
    cfg.port        = static_cast<std::uint16_t>(std::atoi(std::string(_port->getString()).c_str()));
    cfg.account     = std::string(_account->getString());
    cfg.password    = std::string(_password->getString());

    if (!gc.assetsLoaded() || gc.install().options().directory != cfg.uoDirectory)
    {
        setStatus("Loading UO data...");
        if (!gc.loadAssets())
        {
            setStatus(gc.error(), true);
            return;
        }
    }

    cfg.save();
    showChoices({});
    setStatus("Connecting to " + cfg.host + "...");
    gc.connect();
}
