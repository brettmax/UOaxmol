// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Client:
// Game/GameObjects/Views/MobileView.cs (Draw, DrawInternal, CalculateSitAnimation) and the
// frame clock of Game/GameObjects/Mobile.cs (SetAnimation, ProcessAnimation).
//
// Engine-free: builds the list of sprites a mobile is drawn with (shadows, mount, body,
// equipment in paperdoll order), each with its frame, screen offset, mirroring and hue. The
// Axmol client turns the list into sprites; tests can check it directly.
#pragma once

#include "uo/anim/AnimationCache.h"
#include "uo/anim/Equipment.h"
#include "uo/anim/MobileAnimation.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace uo::anim
{

struct MobileDrawInput
{
    uint16_t body      = 0; // Mobile.Graphic
    uint16_t hue       = 0; // Mobile.Hue
    uint8_t direction  = 0; // facing 0..7 after step processing (the step's direction while walking)
    uint8_t animIndex  = 0; // current frame (MobileAnimClock::animIndex)
    bool isDead        = false;
    bool isHidden      = false;
    bool isFemale      = false;
    bool shadows       = true; // profile ShadowsEnabled

    // Hue forced on every part (selection highlight, notoriety, hidden, out of range...);
    // 0 for none. See defaultOverrideHue for the part that does not depend on UI state.
    uint16_t overrideHue = 0;

    MobileAnimState anim;
    MobileEquipment equipment;

    // The seat under the mobile (Mobile.TryGetSittingInfo). The caller looks up chairs on its
    // tile with findChair and leaves this empty while the mobile is stepping; the renderer
    // also ignores it for mounted, flying and non-human bodies.
    std::optional<SittingInfoData> seat;
};

// MobileView's hue rules that need no UI state: hidden mobiles are grey, dead non-humans
// are drawn in the ghost hue.
uint16_t defaultOverrideHue(uint16_t body, bool isDead, bool isHidden);

struct MobileDrawCommand
{
    enum class Kind : uint8_t
    {
        Shadow,
        Mount,
        Body,
        Equipment,
    };

    Kind kind          = Kind::Body;
    Layer layer        = Layer::Invalid; // for Equipment
    const Frame* frame = nullptr;        // null only for an invisible sitting body (layout anchor)
    uint16_t graphic   = 0;              // animation body the frame came from
    uint8_t action     = 0;
    uint8_t direction  = 0; // stored direction 0..4
    uint8_t frameIndex = 0;

    // Top-left of the frame in screen pixels (y down), relative to the mobile's tile anchor
    // (ClassicUO's posX/posY for the tile, before the +22/-3 adjustments, which are applied).
    int x = 0;
    int y = 0;
    bool mirror = false;

    uint16_t hue    = 0;
    bool partialHue = false;
    bool isLight    = false; // the item emits light; add a light at lightX/lightY
    int lightX      = 0;
    int lightY      = 0;

    // Sitting: split ratios for the upper/mid/lower body (DrawCharacterSitted).
    bool sitting = false;
    float sitUpper = 0.f;
    float sitMid   = 0.f;
    float sitLower = 0.f;
};

struct MobileDrawList
{
    std::vector<MobileDrawCommand> commands; // back to front
    // Bounds of the drawn frames relative to the anchor (MobileView.FrameInfo).
    int boundsX      = 0;
    int boundsY      = 0;
    int boundsWidth  = 0;
    int boundsHeight = 0;
    // Light sources carried by the mobile that are not drawn (non-human bodies, AnimID 0).
    std::vector<Layer> undrawnLights;
};

class MobileRenderer
{
public:
    MobileRenderer(AnimationCache& cache, MobileAnimation& actions) : _cache(cache), _actions(actions) {}

    // `mirror` is the mobile's persistent IsFlipped: directions that fold without mirroring
    // (3 and 7) keep the previous value, as in ClassicUO.
    MobileDrawList build(const MobileDrawInput& input, bool& mirror, int anchorX = 0, int anchorY = 0,
                         int offsetX = 0, int offsetY = 0, int offsetZ = 0);

    // MobileView.CheckMouseSelection: pixel-exact hit test of the mount, body and worn items.
    bool hitTest(const MobileDrawList& list, int x, int y) const;

private:
    struct SitState
    {
        int frameStartY = 0;
        int waistY      = 0;
        int kneesY      = 0;
        int feetY       = 0;
        int frameHeight = 0;
    };

    void drawInternal(MobileDrawList& out, const MobileDrawInput& input, const EquippedItem* entity, int x, int y,
                      bool mirror, uint8_t frameIndex, bool hasShadow, uint16_t id, uint8_t animGroup, uint8_t dir,
                      bool isHuman, bool isEquip, bool isMount, int8_t mountOffset, uint16_t overrideHue,
                      bool charIsSitting, const EquipConvData* equipConv, MobileDrawCommand::Kind kind, Layer layer);

    AnimationCache& _cache;
    MobileAnimation& _actions;
    SitState _sit;
};

// Mobile's animation clock: which frame of the current action is showing and when to advance.
struct MobileAnimClock
{
    static constexpr uint32_t CHARACTER_ANIMATION_DELAY = 80; // ms
    static constexpr uint32_t WALKING_DELAY             = 150; // ms (Constants.WALKING_DELAY)

    uint8_t animIndex           = 0;
    uint8_t animationFrameCount = 0;
    uint8_t interval            = 0;
    uint16_t repeatMode         = 1;
    uint16_t repeatModeCount    = 1;
    bool repeat                 = false;
    bool forward                = false;
    uint32_t lastChange         = 0;

    // Mobile.SetAnimation; also used for packets 0x6E and 0xE2. `state` receives the group
    // and fromServer flag the action logic reads.
    void setAnimation(MobileAnimState& state, uint8_t id, uint32_t now, uint8_t intervalValue = 0,
                      uint8_t frameCount = 0, uint16_t repeatCount = 0, bool repeatValue = false,
                      bool forwardValue = false, bool fromServer = false);

    enum class Result : uint8_t
    {
        Waiting,        // not time yet, or not iterating
        Advanced,       // animIndex moved
        NoFrames,       // the action has no frames in this direction
        LoopedToStart,  // a client-driven action wrapped around (a dead corpse-mobile is removed here)
    };

    // Mobile.ProcessAnimation. `frames` is the frame count of the current action/direction;
    // `noIterate` is Mobile.NoIterateAnimIndex (not animating, or standing after a recent step).
    Result tick(MobileAnimState& state, uint32_t now, int frames, bool noIterate);
};

} // namespace uo::anim
