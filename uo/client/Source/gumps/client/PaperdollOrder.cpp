// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/Data/PaperdollOrder.cs and
// Game/GameObjects/Views/MobileView.cs (IsCovered).
#include "gumps/client/PaperdollOrder.h"

#include "uo/world/World.h"

namespace uo::client::gumps::paperdoll_order
{

using uo::world::Item;
using uo::world::Layer;
using uo::world::Mobile;

namespace
{

// T1: arms drawn late.
constexpr Order T1 = {Layer::Invalid,  Layer::Cloak,    Layer::Shirt,   Layer::Pants,    Layer::Shoes,
                      Layer::Legs,     Layer::Torso,    Layer::Tunic,   Layer::Ring,     Layer::Bracelet,
                      Layer::Face,     Layer::Arms,     Layer::Gloves,  Layer::Skirt,    Layer::Robe,
                      Layer::Waist,    Layer::Necklace, Layer::Hair,    Layer::Beard,    Layer::Earrings,
                      Layer::Helmet,   Layer::OneHanded, Layer::TwoHanded, Layer::Backpack, Layer::Talisman};

// T2: the default, arms early.
constexpr Order T2 = {Layer::Invalid,  Layer::Cloak,    Layer::Shirt,   Layer::Pants,    Layer::Shoes,
                      Layer::Legs,     Layer::Arms,     Layer::Torso,   Layer::Tunic,    Layer::Ring,
                      Layer::Bracelet, Layer::Face,     Layer::Gloves,  Layer::Skirt,    Layer::Robe,
                      Layer::Waist,    Layer::Necklace, Layer::Hair,    Layer::Beard,    Layer::Earrings,
                      Layer::Helmet,   Layer::OneHanded, Layer::TwoHanded, Layer::Backpack, Layer::Talisman};

// T3: torso pulled to the front (female-style chest under).
constexpr Order T3 = {Layer::Invalid,  Layer::Cloak,    Layer::Torso,   Layer::Shirt,    Layer::Pants,
                      Layer::Shoes,    Layer::Legs,     Layer::Tunic,   Layer::Ring,     Layer::Bracelet,
                      Layer::Face,     Layer::Arms,     Layer::Gloves,  Layer::Skirt,    Layer::Robe,
                      Layer::Waist,    Layer::Necklace, Layer::Hair,    Layer::Beard,    Layer::Earrings,
                      Layer::Helmet,   Layer::OneHanded, Layer::TwoHanded, Layer::Backpack, Layer::Talisman};

constexpr int idx(Layer l)
{
    return static_cast<int>(l);
}

int indexOf(const Order& a, Layer v)
{
    for (int i = 0; i < N; i++)
    {
        if (a[i] == v)
        {
            return i;
        }
    }

    return -1;
}

// Relocates A so it lands on B's index.
void moveTo(Order& a, Layer A, Layer B)
{
    int i1 = indexOf(a, A);

    if (i1 < 0)
    {
        return;
    }

    int i2 = indexOf(a, B);

    if (i2 < 0 || i2 == i1)
    {
        return;
    }

    if (i2 < i1)
    {
        for (int k = i1; k > i2; k--)
        {
            a[k] = a[k - 1];
        }
    }
    else
    {
        for (int k = i1; k < i2; k++)
        {
            a[k] = a[k + 1];
        }
    }

    a[i2] = A;
}

// Relocates A so it lands immediately after B.
void moveAfter(Order& a, Layer A, Layer B)
{
    int i1 = indexOf(a, A);

    if (i1 < 0)
    {
        return;
    }

    int i2 = indexOf(a, B);

    if (i2 < 0 || i2 == i1)
    {
        return;
    }

    if (i2 < i1)
    {
        for (int k = i1; k > i2 + 1; k--)
        {
            a[k] = a[k - 1];
        }

        a[i2 + 1] = A;
    }
    else
    {
        for (int k = i1; k < i2; k++)
        {
            a[k] = a[k + 1];
        }

        a[i2] = A;
    }
}

uint16_t animOf(uo::world::World& world, const Mobile& mobile, Layer layer, const AnimIdOf& animIdOf)
{
    const Item* item = world.findItemByLayer(mobile, layer);
    return item && animIdOf ? animIdOf(item->graphic) : 0;
}

uint16_t graphicOf(uo::world::World& world, const Mobile& mobile, Layer layer)
{
    const Item* item = world.findItemByLayer(mobile, layer);
    return item ? item->graphic : 0;
}

bool has(uo::world::World& world, const Mobile& mobile, Layer layer)
{
    return world.findItemByLayer(mobile, layer) != nullptr;
}

}  // namespace

bool isGargoyleBody(uint16_t graphic)
{
    return graphic == 0x029A || graphic == 0x029B || graphic == 0x02B6 || graphic == 0x02B7;
}

Graphics graphicsFromMobile(uo::world::World& world, const Mobile& mobile, const AnimIdOf& animIdOf)
{
    Graphics gfx{};

    for (int layer = idx(Layer::OneHanded); layer <= idx(Layer::Legs); layer++)
    {
        if (const Item* item = world.findItemByLayer(mobile, static_cast<Layer>(layer)))
        {
            gfx[layer] = animIdOf ? animIdOf(item->graphic) : 0;
        }
    }

    return gfx;
}

Order build(const Graphics& graphic, bool altTorsoTable)
{
    // 1) Base table. An alternate Arms graphic locks T1 and skips the Torso checks.
    const Order* table = &T2;

    const uint32_t arms = graphic[idx(Layer::Arms)];
    bool armsAlternate;

    if (arms < 0x3d0)
    {
        armsAlternate = arms == 0x3cf || arms == 0x210 || arms == 0x3b3;
    }
    else
    {
        armsAlternate = arms == 0x3dd;
    }

    if (armsAlternate)
    {
        table = &T1;
    }
    else
    {
        const uint32_t torso = graphic[idx(Layer::Torso)];

        if (torso == 0x21a)
        {
            table = &T1;
        }
        else if (torso - 0x399u < 5 && altTorsoTable)
        {
            table = &T3;
        }
    }

    Order o = *table;

    // 2) Per-graphic reorder rules.
    if (graphic[idx(Layer::Shirt)] != 0 && graphic[idx(Layer::Pants)] == 0x398)
    {
        moveTo(o, Layer::Pants, Layer::Shirt);
    }

    const uint32_t pants = graphic[idx(Layer::Pants)];
    bool skipFinalPantsCheck = false;

    if (pants < 0x201)
    {
        if (pants == 0x200 || pants == 0x1eb || pants == 0x1fa)
        {
            // Shoes before pants.
            int iShoes = indexOf(o, Layer::Shoes);
            int iPants = indexOf(o, Layer::Pants);

            if (iShoes >= 0 && iPants >= 0 && iPants < iShoes)
            {
                o[iShoes] = Layer::Pants;
                o[iPants] = Layer::Shoes;
            }
        }
    }
    else if (pants - 0x513u < 2)
    {
        if (graphic[idx(Layer::Shoes)] != 0)
        {
            moveTo(o, Layer::Pants, Layer::Shoes);
        }

        skipFinalPantsCheck = true;
    }

    if (!skipFinalPantsCheck && graphic[idx(Layer::Shoes)] != 0 && graphic[idx(Layer::Pants)] == 0x3e4)
    {
        moveTo(o, Layer::Pants, Layer::Shoes);
    }

    // Tunic 0x238 (surcoat) goes after the waist; with some robes the robe moves to the neck.
    if (graphic[idx(Layer::Tunic)] == 0x238)
    {
        moveAfter(o, Layer::Tunic, Layer::Waist);

        if (graphic[idx(Layer::Robe)] != 0)
        {
            const uint32_t r = graphic[idx(Layer::Robe)];

            if (r == 0x4e8 || r == 0x4e9 || r == 0x4ea || r == 0x4eb || r == 0x5e2 || r == 0x5e3 || r == 0x5e4 ||
                r == 0x5e5)
            {
                moveTo(o, Layer::Robe, Layer::Necklace);
            }
        }
    }

    // Quivers (cloak 0x380 / 0x5F3) draw over the robe.
    const uint32_t cloak = graphic[idx(Layer::Cloak)];

    if (cloak == 0x380 || cloak == 0x5f3)
    {
        moveAfter(o, Layer::Cloak, Layer::Robe);
    }

    // Helmet and necklace.
    const uint32_t helm = graphic[idx(Layer::Helmet)];

    if (helm < 0x202)
    {
        const uint32_t neck = graphic[idx(Layer::Necklace)];

        if ((helm == 0x201 || helm == 0x1a9) && (neck == 0x1c8 || (neck > 0x1d6 && neck < 0x1d9)))
        {
            moveAfter(o, Layer::Necklace, Layer::Helmet);
            return o;
        }
    }
    else if (helm - 0x5e9u < 2 && graphic[idx(Layer::Robe)] != 0 && (graphic[idx(Layer::Robe)] - 0x5e2u) < 4)
    {
        moveTo(o, Layer::Robe, Layer::Helmet);
    }

    return o;
}

int filter(const Order& order, bool includeBackpack, Order& dest)
{
    int c = 0;

    for (Layer layer : order)
    {
        if (layer == Layer::Invalid || layer == Layer::Mount)
        {
            continue;
        }

        if (layer == Layer::Backpack && !includeBackpack)
        {
            continue;
        }

        dest[c++] = layer;
    }

    return c;
}

bool isCovered(uo::world::World& world, const Mobile& mobile, Layer layer, const AnimIdOf& animIdOf)
{
    if (mobile.isEmpty())
    {
        return false;
    }

    // Robe graphics compare raw; pants / skirt / robe anim ids use tiledata. The exception
    // robes are open robes that leave the chest and arms visible.
    const uint16_t robeGfx = graphicOf(world, mobile, Layer::Robe);
    const uint16_t robeAnim = animOf(world, mobile, Layer::Robe, animIdOf);
    const bool hasRobe = has(world, mobile, Layer::Robe);

    const bool robeLeavesChestVisible = robeGfx == 0x9985 || robeGfx == 0x9986 || robeGfx == 0xA2CA ||
                                        robeGfx == 0xA2CB || robeGfx == 0xA412 || robeGfx == 0xB1DE;

    switch (layer)
    {
        case Layer::Shoes:
        {
            const uint16_t pantsAnim = animOf(world, mobile, Layer::Pants, animIdOf);

            if (robeAnim != 0x504 && pantsAnim != 0x513 && pantsAnim != 0x514)
            {
                const uint16_t pantsGfx = graphicOf(world, mobile, Layer::Pants);

                if (pantsGfx < 0xAEB2)
                {
                    if (pantsGfx == 0xAEB1 || pantsGfx == 0x1411)
                    {
                        return true;
                    }

                    if (pantsGfx != 0xAEA2)
                    {
                        // Plate / studded legs paint under shoes.
                        return has(world, mobile, Layer::Legs);
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
                        return has(world, mobile, Layer::Legs);
                    }
                }
            }

            return true;
        }

        case Layer::Pants:
        {
            if (has(world, mobile, Layer::Legs) || robeAnim == 0x504)
            {
                return true;
            }

            const uint16_t pantsAnim = animOf(world, mobile, Layer::Pants, animIdOf);

            if (pantsAnim != 0x1EB && pantsAnim != 0x1FA && pantsAnim != 0x200)
            {
                return false;
            }

            if (has(world, mobile, Layer::Skirt))
            {
                const uint16_t skirtAnim = animOf(world, mobile, Layer::Skirt, animIdOf);

                if (skirtAnim != 0x1C7 && skirtAnim != 0x1E4)
                {
                    return true;
                }
            }

            if (!hasRobe)
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
            // A surcoat (anim 0x238) sits on top of the robe; hide it over a full robe.
            if (has(world, mobile, Layer::Tunic) && animOf(world, mobile, Layer::Tunic, animIdOf) == 0x0238)
            {
                return hasRobe && !robeLeavesChestVisible;
            }

            break;
        }

        case Layer::Torso:
        {
            if (robeGfx != 0 && !robeLeavesChestVisible)
            {
                return true;
            }

            const uint16_t tunicGfx = graphicOf(world, mobile, Layer::Tunic);

            if (has(world, mobile, Layer::Tunic) && tunicGfx != 0x1541 && tunicGfx != 0x1542)
            {
                const uint16_t torsoGfx = graphicOf(world, mobile, Layer::Torso);

                if (has(world, mobile, Layer::Torso) && (torsoGfx == 0x782A || torsoGfx == 0x782B))
                {
                    return true;
                }
            }

            break;
        }

        case Layer::Arms:
            return robeGfx != 0 && !robeLeavesChestVisible;

        case Layer::Necklace:
        {
            if (!hasRobe)
            {
                return false;
            }

            // Open robes that leave a neck item (anim 0x5EC) visible.
            if (robeAnim == 0x5F2 || robeAnim == 0x5F5 || (robeAnim >= 0x4E8 && robeAnim <= 0x4EB) ||
                (robeAnim >= 0x5E2 && robeAnim <= 0x5E5))
            {
                return false;
            }

            return has(world, mobile, Layer::Necklace) && animOf(world, mobile, Layer::Necklace, animIdOf) == 0x5EC;
        }

        case Layer::Bracelet:
            return graphicOf(world, mobile, Layer::Bracelet) == 0xB1C0 && has(world, mobile, Layer::Arms);

        case Layer::Hair:
        case Layer::Helmet:
        {
            if (layer == Layer::Hair)
            {
                const uint16_t helmetGfx = graphicOf(world, mobile, Layer::Helmet);

                if (has(world, mobile, Layer::Helmet) && static_cast<uint32_t>(helmetGfx - 0xA42B) < 2)
                {
                    return true;
                }
            }

            // Hair and helmet hidden under a hood-style robe.
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

            // These hoods cover the head on every body except gargoyles.
            return !isGargoyleBody(mobile.graphic);
        }

        case Layer::Skirt:
        {
            const uint16_t skirtAnim = animOf(world, mobile, Layer::Skirt, animIdOf);

            if (skirtAnim != 0x1C7 && skirtAnim != 0x1E4)
            {
                return false;
            }

            const uint16_t pantsAnim = animOf(world, mobile, Layer::Pants, animIdOf);
            return pantsAnim == 0x1EB || pantsAnim == 0x1FA || pantsAnim == 0x200;
        }

        default:
            break;
    }

    return false;
}

}  // namespace uo::client::gumps::paperdoll_order
