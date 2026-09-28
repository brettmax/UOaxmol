// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (MessageManager.CreateMessage, TextContainer, TextRenderer and
// GameObject/Mobile.UpdateTextCoordsV).

#include "axmol/OverheadTextLayer.h"

#include "axmol/TextSystem.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace uo::client::text
{

OverheadTextLayer* OverheadTextLayer::create()
{
    auto* layer = new OverheadTextLayer();

    if (layer->init())
    {
        layer->autorelease();
        return layer;
    }

    delete layer;
    return nullptr;
}

bool OverheadTextLayer::init()
{
    if (!ax::Node::init())
        return false;

    scheduleUpdate();
    return true;
}

std::int64_t OverheadTextLayer::nowMs()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration_cast<milliseconds>(steady_clock::now() - start).count();
}

void OverheadTextLayer::setViewport(const ax::Rect& viewport)
{
    _viewport = viewport;
    setContentSize(viewport.size);
}

void OverheadTextLayer::addMessage(std::uint32_t serial, std::string_view utf8, std::uint16_t hue, std::uint8_t font,
                                   bool unicode, uo::text::MessageType type, uo::text::TextType textType)
{
    auto& system = TextSystem::instance();

    if (utf8.empty() || !system.ready())
        return;

    const std::u16string s = fontText(utf8, unicode);

    TextStyle style;
    style.font       = font;
    style.unicode    = unicode;
    style.border     = true;
    style.maxWidth   = uo::text::speechLayoutWidth(system.fonts(), system.resolveFont(font), unicode, s);
    style.saveHitMap = textType == uo::text::TextType::Object;

    const std::uint16_t fixed = uo::text::fixSpeechHue(hue);
    style.hue = (!unicode && textType == uo::text::TextType::Object) ? std::uint16_t{0x7FFF} : fixed;

    TextLabel* label = TextLabel::create(utf8, style);

    if (!label)
        return;

    Message message;
    message.label   = label;
    message.owner   = serial;
    message.order   = ++_next;
    message.type    = type;
    message.expires = nowMs() + uo::text::speechTimeToLive(label->lineCount(), _delay);

    addChild(label, static_cast<int>(message.order & 0x3FFFFFFF));
    label->setVisible(false);  // placed on the next update

    auto& stack = _stacks[serial];
    stack.push_back(message);

    while (stack.size() > kMaxMessagesPerOwner)
    {
        destroy(stack.front());
        stack.pop_front();
    }
}

void OverheadTextLayer::destroy(Message& message)
{
    if (message.label)
    {
        message.label->removeFromParent();
        message.label = nullptr;
    }
}

void OverheadTextLayer::removeOwner(std::uint32_t serial)
{
    auto it = _stacks.find(serial);

    if (it == _stacks.end())
        return;

    for (auto& m : it->second)
        destroy(m);

    _stacks.erase(it);
}

void OverheadTextLayer::clear()
{
    for (auto& [owner, stack] : _stacks)
    {
        for (auto& m : stack)
            destroy(m);
    }

    _stacks.clear();
}

void OverheadTextLayer::layoutOwner(std::uint32_t owner, std::deque<Message>& stack, std::int64_t now)
{
    const std::optional<ax::Vec2> anchor = _anchor ? _anchor(owner) : std::nullopt;

    for (auto& m : stack)
        m.placed = false;

    if (!anchor)
        return;

    // Newest first, stacking upward. Expired messages at the bottom of the stack are
    // skipped so the rest drop down to the head.
    float offY = 0.0f;

    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
    {
        const ax::Size& size = it->label->getContentSize();

        if (offY == 0.0f && it->expires < now)
            continue;

        offY += size.height;
        it->position = ax::Vec2(anchor->x - static_cast<float>(static_cast<int>(size.width) >> 1), anchor->y - offY);
        it->placed   = true;
    }

    // FixTextCoordinatesInScreen: keep live text inside the viewport.
    const float left   = _viewport.origin.x + 4.0f;
    const float top    = _viewport.origin.y;
    const float right  = _viewport.origin.x + _viewport.size.width;
    const float bottom = _viewport.origin.y + _viewport.size.height - _bottomInset;

    for (auto& m : stack)
    {
        if (!m.placed || m.expires < now)
            continue;

        const ax::Size& size = m.label->getContentSize();

        if (m.position.x < left)
            m.position.x = left;
        if (m.position.x + size.width > right)
            m.position.x -= m.position.x + size.width - right;
        if (m.position.y < top)
            m.position.y = top;
        if (m.position.y + size.height > bottom)
            m.position.y -= m.position.y + size.height - bottom;
    }
}

void OverheadTextLayer::place(Message& m)
{
    // UO screen (y down) to this node's space (origin bottom-left of the viewport); labels
    // are anchored at their top-left corner.
    m.label->setPosition(m.position.x - _viewport.origin.x,
                         _viewport.size.height - (m.position.y - _viewport.origin.y));
}

void OverheadTextLayer::update(float delta)
{
    (void)delta;
    const std::int64_t now = nowMs();

    for (auto it = _stacks.begin(); it != _stacks.end();)
    {
        auto& stack = it->second;

        // Drop text whose time is up; the original stops drawing it at that point.
        while (!stack.empty() && stack.front().expires < now)
        {
            destroy(stack.front());
            stack.pop_front();
        }

        if (stack.empty())
        {
            it = _stacks.erase(it);
            continue;
        }

        layoutOwner(it->first, stack, now);
        ++it;
    }

    // TextRenderer.ProcessWorldText + CalculateAlpha: newest text first, anything that
    // overlaps text already placed turns transparent; the last second fades out.
    std::vector<Message*> live;

    for (auto& [owner, stack] : _stacks)
    {
        for (auto& m : stack)
        {
            if (m.placed && m.expires >= now)
                live.push_back(&m);
            else
                m.label->setVisible(false);
        }
    }

    std::sort(live.begin(), live.end(), [](const Message* a, const Message* b) { return a->order > b->order; });

    std::vector<ax::Rect> bounds;
    bounds.reserve(live.size());

    for (Message* m : live)
    {
        const ax::Size& size = m->label->getContentSize();
        const ax::Rect rect(m->position.x, m->position.y, size.width, size.height);

        m->transparent = std::any_of(bounds.begin(), bounds.end(), [&](const ax::Rect& r) {
            return r.intersectsRect(rect);
        });
        bounds.push_back(rect);

        if (_textFading)
        {
            int left = static_cast<int>(m->expires - now);

            if (left >= 0 && left <= 1000)
            {
                left /= 10;
                left = std::clamp(left, 0, 100);
                left = 255 * left / 100;

                if (!m->transparent || left <= 0x7F)
                    m->alpha = static_cast<std::uint8_t>(left);

                m->transparent = true;
            }
        }

        std::uint8_t alpha = m->alpha;

        if (m->transparent && alpha == 0xFF)
            alpha = 0x7F;

        m->label->setOpacity(alpha);
        m->label->setVisible(true);
        place(*m);
    }
}

std::optional<std::uint32_t> OverheadTextLayer::hitTest(const ax::Vec2& uoPoint) const
{
    const Message* best = nullptr;

    for (const auto& [owner, stack] : _stacks)
    {
        for (const auto& m : stack)
        {
            if (!m.label->isVisible())
                continue;

            const int x = static_cast<int>(uoPoint.x - m.position.x);
            const int y = static_cast<int>(uoPoint.y - m.position.y);

            if (m.label->hitTest(x, y) && (!best || m.order > best->order))
                best = &m;
        }
    }

    return best ? std::optional<std::uint32_t>(best->owner) : std::nullopt;
}

}  // namespace uo::client::text
