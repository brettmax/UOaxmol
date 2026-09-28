// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"
#include "axmol/ui/CocosGUI.h"

#include <vector>

// Account login, shard selection and character selection. The Axmol version of ClassicUO's
// LoginScene and its LoginGump/ServerSelectionGump/CharacterSelectionGump, kept to plain
// widgets until the gump UI port can draw the classic art.
class LoginScene : public ax::Scene
{
public:
    bool init() override;
    void onEnter() override;
    void onExit() override;
    void update(float dt) override;

private:
    ax::ui::InputField* addField(const char* label, std::string_view value, float y, bool password = false);
    void setStatus(std::string_view text, bool error = false);
    void login();
    void showChoices(const std::vector<std::pair<std::string, std::function<void()>>>& choices);

    ax::ui::InputField* _dir      = nullptr;
    ax::ui::InputField* _host     = nullptr;
    ax::ui::InputField* _port     = nullptr;
    ax::ui::InputField* _account  = nullptr;
    ax::ui::InputField* _password = nullptr;
    ax::Label* _status            = nullptr;
    ax::Menu* _choices            = nullptr;
};
