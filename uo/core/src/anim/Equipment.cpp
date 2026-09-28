// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO: Game/Data/PaperdollOrder.cs, Game/Data/Mounts.cs,
// Game/Data/ChairTable.cs, Game/GameObjects/Views/MobileView.cs (IsCovered,
// FixGargoyleEquipments) and Game/GameObjects/Item.cs (GetGraphicForAnimation).
#include "uo/anim/Equipment.h"

#include <charconv>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::anim
{

namespace PaperdollOrder
{

namespace
{

using L = Layer;

// T1: arms drawn late
constexpr Layer T1[N] = {L::Invalid, L::Cloak,    L::Shirt,  L::Pants,    L::Shoes,     L::Legs,      L::Torso,
                         L::Tunic,   L::Ring,     L::Bracelet, L::Face,   L::Arms,      L::Gloves,    L::Skirt,
                         L::Robe,    L::Waist,    L::Necklace, L::Hair,   L::Beard,     L::Earrings,  L::Helmet,
                         L::OneHanded, L::TwoHanded, L::Backpack, L::Talisman};

// T2: default, arms early
constexpr Layer T2[N] = {L::Invalid, L::Cloak,    L::Shirt,  L::Pants,    L::Shoes,     L::Legs,      L::Arms,
                         L::Torso,   L::Tunic,    L::Ring,   L::Bracelet, L::Face,      L::Gloves,    L::Skirt,
                         L::Robe,    L::Waist,    L::Necklace, L::Hair,   L::Beard,     L::Earrings,  L::Helmet,
                         L::OneHanded, L::TwoHanded, L::Backpack, L::Talisman};

// T3: torso pulled to front (female-style chest-under)
constexpr Layer T3[N] = {L::Invalid, L::Cloak,    L::Torso,  L::Shirt,    L::Pants,     L::Shoes,     L::Legs,
                         L::Tunic,   L::Ring,     L::Bracelet, L::Face,   L::Arms,      L::Gloves,    L::Skirt,
                         L::Robe,    L::Waist,    L::Necklace, L::Hair,   L::Beard,     L::Earrings,  L::Helmet,
                         L::OneHanded, L::TwoHanded, L::Backpack, L::Talisman};

int indexOf(std::span<const Layer> a, Layer v)
{
    for (int i = 0; i < N; ++i)
    {
        if (a[i] == v)
        {
            return i;
        }
    }
    return -1;
}

// Relocate value a so it lands on b's index.
void moveTo(std::span<Layer> arr, Layer a, Layer b)
{
    const int i1 = indexOf(arr, a);
    if (i1 < 0)
    {
        return;
    }
    const int i2 = indexOf(arr, b);
    if (i2 < 0 || i2 == i1)
    {
        return;
    }
    if (i2 < i1)
    {
        for (int k = i1; k > i2; --k)
        {
            arr[k] = arr[k - 1];
        }
    }
    else
    {
        for (int k = i1; k < i2; ++k)
        {
            arr[k] = arr[k + 1];
        }
    }
    arr[i2] = a;
}

// Relocate value a so it lands immediately after b.
void moveAfter(std::span<Layer> arr, Layer a, Layer b)
{
    const int i1 = indexOf(arr, a);
    if (i1 < 0)
    {
        return;
    }
    const int i2 = indexOf(arr, b);
    if (i2 < 0 || i2 == i1)
    {
        return;
    }
    if (i2 < i1)
    {
        for (int k = i1; k > i2 + 1; --k)
        {
            arr[k] = arr[k - 1];
        }
        arr[i2 + 1] = a;
    }
    else
    {
        for (int k = i1; k < i2; ++k)
        {
            arr[k] = arr[k + 1];
        }
        arr[i2] = a;
    }
}

uint32_t g(std::span<const uint16_t> graphic, Layer l)
{
    return graphic[static_cast<size_t>(l)];
}

} // namespace

void build(std::span<const uint16_t> graphic, bool altTorsoTable, std::span<Layer> order)
{
    // 1) base table selection. An Arms-graphic alternate match locks T1 and skips the
    //    Torso/race checks.
    const Layer* table = T2;

    const uint32_t arms = g(graphic, L::Arms);
    const bool armsAlternate = arms < 0x3d0 ? (arms == 0x3cf || arms == 0x210 || arms == 0x3b3) : arms == 0x3dd;

    if (armsAlternate)
    {
        table = T1;
    }
    else
    {
        const uint32_t torso = g(graphic, L::Torso);
        if (torso == 0x21a)
        {
            table = T1;
        }
        else if (torso - 0x399u < 5 && altTorsoTable)
        {
            table = T3;
        }
    }

    for (int i = 0; i < N; ++i)
    {
        order[i] = table[i];
    }
    auto o = order.first(N);

    // 2) per-graphic reorder rules
    if (g(graphic, L::Shirt) != 0 && g(graphic, L::Pants) == 0x398)
    {
        moveTo(o, L::Pants, L::Shirt);
    }

    const uint32_t pants     = g(graphic, L::Pants);
    bool skipFinalPantsCheck = false;
    if (pants < 0x201)
    {
        if (pants == 0x200 || pants == 0x1eb || pants == 0x1fa)
        {
            // ensure Shoes before Pants
            const int iShoes = indexOf(o, L::Shoes);
            const int iPants = indexOf(o, L::Pants);
            if (iShoes >= 0 && iPants >= 0 && iPants < iShoes)
            {
                o[iShoes] = L::Pants;
                o[iPants] = L::Shoes;
            }
        }
    }
    else if (pants - 0x513u < 2)
    {
        if (g(graphic, L::Shoes) != 0)
        {
            moveTo(o, L::Pants, L::Shoes);
        }
        skipFinalPantsCheck = true;
    }

    if (!skipFinalPantsCheck && g(graphic, L::Shoes) != 0 && g(graphic, L::Pants) == 0x3e4)
    {
        moveTo(o, L::Pants, L::Shoes);
    }

    // tunic 0x238 -> Tunic after Waist; with certain robes, Robe onto Necklace's slot
    if (g(graphic, L::Tunic) == 0x238)
    {
        moveAfter(o, L::Tunic, L::Waist);
        const uint32_t r = g(graphic, L::Robe);
        if (r != 0 && ((r >= 0x4e8 && r <= 0x4eb) || (r >= 0x5e2 && r <= 0x5e5)))
        {
            moveTo(o, L::Robe, L::Necklace);
        }
    }

    // cloak/quiver 0x380/0x5F3 -> Cloak after Robe
    const uint32_t cloak = g(graphic, L::Cloak);
    if (cloak == 0x380 || cloak == 0x5f3)
    {
        moveAfter(o, L::Cloak, L::Robe);
    }

    // helmet/neck interplay
    const uint32_t helm = g(graphic, L::Helmet);
    if (helm < 0x202)
    {
        const uint32_t neck = g(graphic, L::Necklace);
        if ((helm == 0x201 || helm == 0x1a9) && (neck == 0x1c8 || (neck > 0x1d6 && neck < 0x1d9)))
        {
            moveAfter(o, L::Necklace, L::Helmet);
            return;
        }
    }
    else if (helm - 0x5e9u < 2 && g(graphic, L::Robe) != 0 && (g(graphic, L::Robe) - 0x5e2u) < 4)
    {
        moveTo(o, L::Robe, L::Helmet);
    }
}

int filter(std::span<const Layer> order, bool includeBackpack, std::span<Layer> dest)
{
    int c = 0;
    for (Layer layer : order)
    {
        if (layer == L::Invalid || layer == L::Mount)
        {
            continue;
        }
        if (layer == L::Backpack && !includeBackpack)
        {
            continue;
        }
        dest[c++] = layer;
    }
    return c;
}

int applyDirectionCloak(std::span<Layer> layers, int count, uint8_t dir)
{
    int idx = -1;
    for (int i = 0; i < count; ++i)
    {
        if (layers[i] == L::Cloak)
        {
            idx = i;
            break;
        }
    }

    if (idx < 0)
    {
        return count;
    }

    for (int i = idx; i < count - 1; ++i)
    {
        layers[i] = layers[i + 1];
    }
    --count;

    int insert = count;
    if (dir == 0)
    {
        insert = count; // painted last = on top
    }
    else if (dir == 3)
    {
        insert = 0; // painted first = behind
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            if (layers[i] == L::Helmet)
            {
                insert = i;
                break;
            }
        }
    }

    for (int i = count; i > insert; --i)
    {
        layers[i] = layers[i - 1];
    }
    layers[insert] = L::Cloak;
    return count + 1;
}

int buildInWorld(const MobileEquipment& equipment, bool altTorsoTable, uint8_t direction, std::span<Layer> dest)
{
    // Keyed on the equip AnimIDs, not the world graphics: bone arms 0x1410/0x1417 both
    // resolve to AnimID 0x210, which selects the arms-late table.
    std::array<uint16_t, N> gfx{};
    for (int layer = static_cast<int>(L::OneHanded); layer <= static_cast<int>(L::Legs); ++layer)
    {
        if (const auto* item = equipment.find(static_cast<Layer>(layer)))
        {
            gfx[layer] = item->animId;
        }
    }

    std::array<Layer, N> order{};
    build(gfx, altTorsoTable, order);

    const int count = filter(order, false, dest);
    return applyDirectionCloak(dest, count, direction);
}

} // namespace PaperdollOrder

bool isCovered(const MobileEquipment& equipment, uint16_t body, Layer layer)
{
    if (equipment.empty())
    {
        return false;
    }

    // Explicit per-layer occlusion: the paint order alone is not enough, because oversized
    // custom art can leak out from under the item meant to cover it. Robe graphics are
    // compared raw; pants/skirt/robe AnimIDs use the equip AnimID.
    const EquippedItem* robe  = equipment.find(Layer::Robe);
    const uint16_t robeGfx    = robe ? robe->graphic : 0;
    const uint16_t robeAnim   = robe ? robe->animId : 0;

    const bool robeLeavesChestVisible = robeGfx == 0x9985 || robeGfx == 0x9986 || robeGfx == 0xA2CA ||
                                        robeGfx == 0xA2CB || robeGfx == 0xA412 || robeGfx == 0xB1DE;

    auto animOf = [&](Layer l) -> uint16_t {
        const auto* it = equipment.find(l);
        return it ? it->animId : 0;
    };
    auto graphicOf = [&](Layer l) -> uint16_t {
        const auto* it = equipment.find(l);
        return it ? it->graphic : 0;
    };

    auto helmetCovered = [&]() -> bool {
        // hair/helmet hidden under a hood-style robe
        if (robeGfx < 0x4B9E)
        {
            if (robeGfx != 0x4B9D)
            {
                if (robeGfx < 0x2FBA)
                {
                    if (robeGfx != 0x2FB9)
                    {
                        if (robeGfx > 0x2687)
                        {
                            return false;
                        }
                        if (robeGfx < 0x2683 && (robeGfx < 0x204E || robeGfx > 0x204F))
                        {
                            return false;
                        }
                    }
                }
                else if (robeGfx != 0x3173)
                {
                    return false;
                }
                return true;
            }
        }
        else if (robeGfx < 0xA0B0)
        {
            if (robeGfx < 0xA0AB && robeGfx != 0x7816)
            {
                return false;
            }
        }
        else if (robeGfx != 0xB2B7)
        {
            return false;
        }

        // these hoods cover the head on every body except gargoyles
        const bool gargoyle = body == 0x029A || body == 0x029B || body == 0x02B6 || body == 0x02B7;
        return !gargoyle;
    };

    switch (layer)
    {
    case Layer::Shoes:
    {
        const uint16_t pantsAnim = animOf(Layer::Pants);
        if (robeAnim != 0x504 && pantsAnim != 0x513 && pantsAnim != 0x514)
        {
            const uint16_t pantsGfx = graphicOf(Layer::Pants);
            if (pantsGfx < 0xAEB2)
            {
                if (pantsGfx == 0xAEB1 || pantsGfx == 0x1411)
                {
                    return true;
                }
                if (pantsGfx != 0xAEA2)
                {
                    // plate/studded legs paint under shoes
                    return equipment.find(Layer::Legs) != nullptr;
                }
            }
            else
            {
                if (pantsGfx == 0xAEC0)
                {
                    return true;
                }
                if (pantsGfx != 0xAECF)
                {
                    return equipment.find(Layer::Legs) != nullptr;
                }
            }
        }
        return true;
    }

    case Layer::Pants:
    {
        if (equipment.find(Layer::Legs) != nullptr || robeAnim == 0x504)
        {
            return true;
        }

        const uint16_t pantsAnim = animOf(Layer::Pants);
        if (pantsAnim != 0x1EB && pantsAnim != 0x1FA && pantsAnim != 0x200)
        {
            return false;
        }

        if (const auto* skirt = equipment.find(Layer::Skirt))
        {
            if (skirt->animId != 0x1C7 && skirt->animId != 0x1E4)
            {
                return true;
            }
        }

        if (!robe)
        {
            return false;
        }

        if (robeAnim < 0x4EC)
        {
            if (robeAnim > 0x4E7)
            {
                return false;
            }
            return robeAnim != 0x229;
        }

        return static_cast<uint32_t>(robeAnim - 0x5E2) > 3;
    }

    case Layer::Tunic:
    {
        // tunic AnimID 0x238 is moved on top of the robe (surcoat); hide it under a full robe
        if (const auto* tunic = equipment.find(Layer::Tunic); tunic && tunic->animId == 0x0238)
        {
            return robe != nullptr && !robeLeavesChestVisible;
        }
        return false;
    }

    case Layer::Torso:
    {
        if (robeGfx != 0 && !robeLeavesChestVisible)
        {
            return true;
        }

        const auto* tunic = equipment.find(Layer::Tunic);
        if (tunic && tunic->graphic != 0x1541 && tunic->graphic != 0x1542)
        {
            const auto* torso = equipment.find(Layer::Torso);
            if (torso && (torso->graphic == 0x782A || torso->graphic == 0x782B))
            {
                return true;
            }
        }
        return false;
    }

    case Layer::Arms: return robeGfx != 0 && !robeLeavesChestVisible;

    case Layer::Necklace:
    {
        if (!robe)
        {
            return false;
        }

        // open/half robes that leave a neck item (AnimID 0x5EC) visible
        if (robeAnim == 0x5F2 || robeAnim == 0x5F5 || (robeAnim >= 0x4E8 && robeAnim <= 0x4EB) ||
            (robeAnim >= 0x5E2 && robeAnim <= 0x5E5))
        {
            return false;
        }

        const auto* neck = equipment.find(Layer::Necklace);
        return neck && neck->animId == 0x5EC;
    }

    case Layer::Bracelet:
    {
        const auto* bracelet = equipment.find(Layer::Bracelet);
        return bracelet && bracelet->graphic == 0xB1C0 && equipment.find(Layer::Arms) != nullptr;
    }

    case Layer::Hair:
    {
        const auto* helmet = equipment.find(Layer::Helmet);
        if (helmet && static_cast<uint32_t>(helmet->graphic - 0xA42B) < 2)
        {
            return true;
        }
        return helmetCovered();
    }

    case Layer::Helmet: return helmetCovered();

    case Layer::Skirt:
    {
        const uint16_t skirtAnim = animOf(Layer::Skirt);
        if (skirtAnim != 0x1C7 && skirtAnim != 0x1E4)
        {
            return false;
        }
        const uint16_t pantsAnim = animOf(Layer::Pants);
        return pantsAnim == 0x1EB || pantsAnim == 0x1FA || pantsAnim == 0x200;
    }

    default: return false;
    }
}

uint16_t fixGargoyleEquipment(uint16_t graphic)
{
    switch (graphic)
    {
    case 0x01D5: return 0x0156; // gargoyle robe
    case 0x03CA: return 0x0223; // gargoyle dead shroud
    case 0x03D8: return 329;    // gargoyle spellbook
    case 0x0372: return 330;    // gargoyle necrobook
    case 0x0374: return 328;    // gargoyle chivalry book
    case 0x036F: return 327;    // gargoyle bushido book
    case 0x036E: return 328;    // gargoyle ninjitsu book
    case 0x0426: return 0x042B; // gargoyle masteries book
    // mobtypes.txt of 7.0.90+ lists four tiledata infos for the pirate shield (0xA649);
    // the official client maps the human ones to the gargoyle ones.
    case 1529: return 1531; // EQUIP_Shield_Pirate_Male_H
    case 1530: return 1532; // EQUIP_Shield_Pirate_Female_H
    default: return graphic;
    }
}

namespace
{

struct MountRow
{
    uint16_t item;
    uint16_t body;
    int8_t offsetY;
};

constexpr MountRow kMounts[] = {
#include "Mounts.inc"
};

constexpr SittingInfoData kDefaultChairs[] = {
#include "ChairTable.inc"
};

std::unordered_map<uint16_t, SittingInfoData>& chairTable()
{
    static std::unordered_map<uint16_t, SittingInfoData> table = [] {
        std::unordered_map<uint16_t, SittingInfoData> t;
        for (const auto& c : kDefaultChairs)
        {
            t.emplace(c.graphic, c);
        }
        return t;
    }();
    return table;
}

template <typename T>
T parseOr(std::string_view s, T fallback)
{
    int v          = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size())
    {
        return fallback;
    }
    if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max())
    {
        return fallback;
    }
    return static_cast<T>(v);
}

} // namespace

std::optional<MountInfo> findMount(uint16_t itemGraphic)
{
    for (const auto& m : kMounts)
    {
        if (m.item == itemGraphic)
        {
            return MountInfo{m.body, m.offsetY};
        }
    }
    return std::nullopt;
}

uint16_t mountGraphicForAnimation(uint16_t graphic, uint16_t itemAnimId)
{
    // ethereal unicorn
    if (graphic == 0x3E9B || graphic == 0x3E9D)
    {
        return 0x00C0;
    }

    // ethereal kirin
    if (graphic == 0x3E9C)
    {
        return 0x00BF;
    }

    if (auto mount = findMount(graphic))
    {
        graphic = mount->graphic;
    }

    if (itemAnimId != 0)
    {
        graphic = itemAnimId;
    }

    return graphic;
}

std::optional<SittingInfoData> findChair(uint16_t graphic)
{
    const auto& t = chairTable();
    auto it       = t.find(graphic);
    return it == t.end() ? std::nullopt : std::optional<SittingInfoData>(it->second);
}

void resetChairTable()
{
    auto& t = chairTable();
    t.clear();
    for (const auto& c : kDefaultChairs)
    {
        t.emplace(c.graphic, c);
    }
}

int loadChairTable(std::string_view text)
{
    auto& t = chairTable();
    t.clear();

    int count    = 0;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        start                 = end + 1;

        if (const auto c = line.find_first_of("#;"); c != std::string_view::npos)
        {
            line = line.substr(0, c);
        }

        std::vector<std::string_view> tokens;
        size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == ',' || line[i] == '\r'))
            {
                ++i;
            }
            size_t j = i;
            while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != ',' && line[j] != '\r')
            {
                ++j;
            }
            if (j > i)
            {
                tokens.push_back(line.substr(i, j - i));
            }
            i = j;
        }

        if (tokens.size() < 7)
        {
            continue;
        }

        // Unparsable fields read as 0, like ClassicUO's TryParse calls.
        SittingInfoData d;
        d.graphic       = parseOr<uint16_t>(tokens[0], 0);
        d.direction1    = parseOr<int8_t>(tokens[1], 0);
        d.direction2    = parseOr<int8_t>(tokens[2], 0);
        d.direction3    = parseOr<int8_t>(tokens[3], 0);
        d.direction4    = parseOr<int8_t>(tokens[4], 0);
        d.offsetY       = parseOr<int8_t>(tokens[5], 0);
        d.mirrorOffsetY = parseOr<int8_t>(tokens[6], 0);
        d.drawBack      = false;

        if (t.emplace(d.graphic, d).second)
        {
            ++count;
        }
    }

    return count;
}

} // namespace uo::anim
