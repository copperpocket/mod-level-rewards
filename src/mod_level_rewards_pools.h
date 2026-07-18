#ifndef MOD_LEVEL_REWARDS_POOLS_H
#define MOD_LEVEL_REWARDS_POOLS_H

#include "Define.h"
#include <vector>

// Rarity band for an item inside a pool. Determines its relative draw weight.
enum LootRarity : uint8
{
    RARITY_COMMON,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY, // NOT a pool ticket
    RARITY_MYTHIC,    // NOT a pool ticket
    RARITY_UNIQUE     // NOT a pool ticket
};

// Faction eligibility for a pool item. Default is BOTH so existing entries
// (pets, toys, tabards - none faction-locked) need no change.
enum LootFaction : uint8
{
    FACTION_BOTH,      // usable by Alliance and Horde
    FACTION_ALLIANCE,  // Alliance-only (skipped for Horde players)
    FACTION_HORDE      // Horde-only (skipped for Alliance players)
};

// How often each rarity BAND is chosen (relative; auto-normalized among the
// bands that actually contain eligible items in a given pool).
inline uint32 GetBandWeight(LootRarity r)
{
    switch (r)
    {
        case RARITY_COMMON:    return 500; // ~50%
        case RARITY_UNCOMMON:  return 300; // ~30%
        case RARITY_RARE:      return 150; // ~15%
        case RARITY_EPIC:      return 50;  // ~5%
        default:               return 0;
    }
}

// Relative "ticket count" per rarity band. An item's real chance is its own
// weight divided by the sum of all weights in the same pool. Tune globally.
inline uint32 GetRarityWeight(LootRarity r)
{
    switch (r)
    {
        case RARITY_COMMON:    return 10000;
        case RARITY_UNCOMMON:  return 3000;
        case RARITY_RARE:      return 800;
        case RARITY_EPIC:      return 150;
        default:               return 0; // Legendary/Mythic/Unique
    }
}

// A single entry in a loot pool: item, rarity, and faction eligibility.
struct PoolItem
{
    uint32      itemId;
    LootRarity  rarity;
    LootFaction faction = FACTION_BOTH; // default keeps every existing line valid
};

using LootPool = std::vector<PoolItem>;

// Identifies which themed pool to draw from. Decoupled from ChestTier so any
// tier can pull from any pool.
enum LootPoolId
{
    POOL_WHITE,
    POOL_GREEN,
    POOL_BLUE,
    POOL_PURPLE_60,
    POOL_PURPLE_70,
    POOL_PURPLE_80,
    POOL_MYTHIC
};

// ---------------------------------------------------------------------------
// POOL DATA
// Add/remove items here and tag a rarity. Weights are derived - never touch a
// number. Keep the comments so pull requests stay readable.
// ---------------------------------------------------------------------------

// WHITE POOL - cheap flavor toys + a starter bag
static const LootPool Pool_White = {
    // COMMON
    { 10683, RARITY_COMMON },   // Explorer's Knapsack (16 slot)
    // UNCOMMON
    { 33219, RARITY_UNCOMMON },   // Goblin Gumbo Kettle
    { 33223, RARITY_UNCOMMON },   // Fishing Chair
    { 38301, RARITY_UNCOMMON },   // D.I.S.C.O.
    { 32588, RARITY_UNCOMMON },   // Banana Charm
    { 23713, RARITY_UNCOMMON },   // Hippogryph Hatchling   (pet)
    { 20371, RARITY_UNCOMMON },   // Blue Murloc Egg        (pet)
    { 46802, RARITY_UNCOMMON },   // Heavy Murloc Egg       (pet)
    // RARE
    { 23720, RARITY_RARE },       // Riding Turtle
};

// GREEN POOL - pets & toys (Explorer's Knapsack removed; it lives in White)
static const LootPool Pool_Green = {
    // --- COMMON ---
    { 10683, RARITY_COMMON },   // Explorer's Knapsack (16 slot)
    // --- UNCOMMON: pets ---
    { 30360, RARITY_UNCOMMON }, // Lurky's Egg
    { 49362, RARITY_UNCOMMON }, // Onyxian Whelpling
    { 44819, RARITY_UNCOMMON }, // Baby Blizzard Bear
    { 34492, RARITY_UNCOMMON }, // Rocket Chicken
    { 13582, RARITY_UNCOMMON }, // Zergling Leash
    { 13583, RARITY_UNCOMMON }, // Panda Collar
    { 25535, RARITY_UNCOMMON }, // Netherwhelp's Collar
    { 39286, RARITY_UNCOMMON }, // Frosty's Collar
    // --- UNCOMMON: toys ---
    { 5462,  RARITY_UNCOMMON }, // Dartol's Rod of Transformation
    { 32566, RARITY_UNCOMMON }, // Picnic Basket
    { 34499, RARITY_UNCOMMON }, // Paper Flying Machine Kit
    { 45063, RARITY_UNCOMMON }, // Foam Sword Rack
    { 46780, RARITY_UNCOMMON }, // Ogre Pinata
    { 34493, RARITY_UNCOMMON }, // Dragon Kite
    { 40601, RARITY_UNCOMMON }, // Argent Dawn Banner
    { 32542, RARITY_UNCOMMON }, // Imp in a Ball
    { 38578, RARITY_UNCOMMON }, // The Flag of Ownership
    { 54452, RARITY_UNCOMMON }, // Ethereal Portal
    { 13584, RARITY_UNCOMMON }, // Diablo Stone
    { 49287, RARITY_UNCOMMON }, // Tuskarr Kite
    // --- RARE: pets ---
    { 46894, RARITY_RARE },     // Enchanted Jade
    { 49664, RARITY_RARE },     // Enchanted Purple Jade
    { 34518, RARITY_RARE },     // Golden Pig Coin
    { 34519, RARITY_RARE },     // Silver Pig Coin
    { 37297, RARITY_RARE },     // Gold Medallion
    { 39656, RARITY_RARE },     // Tyrael's Hilt
    { 56806, RARITY_RARE },     // Mini-Thor
    { 49343, RARITY_RARE },     // Spectral Tiger Cub
    { 38050, RARITY_RARE },     // Soul-Trader Beacon
    // --- RARE: toys ---
    { 40110, RARITY_RARE },     // Haunted Memento
    { 35227, RARITY_RARE },     // Goblin Weather Machine - Prototype
    // --- RARE: shirts ---
    { 45037, RARITY_RARE },     // Epic Purple Shirt
    // --- RARE: mounts ---
    { 23720, RARITY_RARE },      // Riding Turtle
};

// BLUE POOL - tabards + classic TCG mounts
static const LootPool Pool_Blue = {
    // --- UNCOMMON: tabards ---
    { 19160, RARITY_UNCOMMON }, // Contest Winner's Tabard
    { 23705, RARITY_UNCOMMON }, // Tabard of Flame
    { 36941, RARITY_UNCOMMON }, // Competitor's Tabard
    { 23720, RARITY_UNCOMMON }, // Riding Turtle
    // --- RARE: tabards ---
    { 28788, RARITY_RARE },     // Tabard of the Protector
    { 22999, RARITY_RARE },     // Tabard of the Argent Dawn
    // --- RARE: mounts ---
    { 8630,  RARITY_RARE },     // Reins of the Bengal Tiger
    { 8633,  RARITY_RARE },     // Reins of the Leopard
};

// PURPLE 60 POOL - classic-era mounts
static const LootPool Pool_Purple60 = {
    // --- COMMON ---
    { 12353, RARITY_COMMON,   FACTION_ALLIANCE }, // White Stallion Bridle (Human)
    { 12354, RARITY_COMMON,   FACTION_ALLIANCE }, // Palomino Bridle (Human)
    { 13326, RARITY_COMMON,   FACTION_ALLIANCE }, // White Mechanostrider Mod A (Gnome)
    { 13327, RARITY_COMMON,   FACTION_ALLIANCE }, // Icy Blue Mechanostrider Mod A (Gnome)
    { 12302, RARITY_COMMON,   FACTION_ALLIANCE }, // Reins of the Frostsaber (Night Elf)
    { 12303, RARITY_COMMON,   FACTION_ALLIANCE }, // Reins of the Nightsaber(Night Elf)
    { 12351, RARITY_COMMON,   FACTION_HORDE }, // Horn of the Arctic Wolf (Orc)
    { 12330, RARITY_COMMON,   FACTION_HORDE }, // Horn of the Red Wolf (Orc)
    { 8586,  RARITY_COMMON, FACTION_HORDE },    // Whistle of the Mottled Red Raptor (Troll)
    { 13317, RARITY_COMMON, FACTION_HORDE },    // Whistle of the Ivory Raptor (Troll)
    { 15293, RARITY_COMMON, FACTION_HORDE },    // Teal Kodo (Tauren)
    { 15292, RARITY_COMMON, FACTION_HORDE },    // Green Kodo (Tauren)
    { 13328, RARITY_COMMON, FACTION_ALLIANCE }, // Black Ram (Dwarf)
    { 13329, RARITY_COMMON,     FACTION_ALLIANCE },  // Frost Ram (Dwarf)
    // --- EPIC ---
    { 13325, RARITY_EPIC,     FACTION_ALLIANCE }, // Fluorescent Green Mechanostrider (Gnome)
};

// PURPLE 70 POOL - TBC/WotLK mounts
static const LootPool Pool_Purple70 = {
    // --- COMMON ---
    { 43599, RARITY_COMMON },   // Big Blizzard Bear
    { 49282, RARITY_COMMON },   // Big Battle Bear
    // --- EPIC ---
    { 49290, RARITY_EPIC },     // Magic Rooster Egg
    { 49284, RARITY_EPIC },     // Reins of the Swift Spectral Tiger
};

// PURPLE 80 POOL - endgame mounts
static const LootPool Pool_Purple80 = {
    // --- COMMON ---
    { 49286, RARITY_COMMON },   // X-51 Nether-Rocket X-TREME
    { 37719, RARITY_COMMON },   // Swift Zhevra
    { 54860, RARITY_COMMON },   // X-53 Touring Rocket
    // --- UNCOMMON ---
    { 44164, RARITY_UNCOMMON }, // Reins of the Black Proto-Drake
    { 44175, RARITY_UNCOMMON }, // Reins of the Plagued Proto-Drake
    { 54069, RARITY_UNCOMMON }, // Blazing Hippogryph
    { 54068, RARITY_UNCOMMON },  // Wooly White Rhino
};

// MYTHIC POOL - drawn only when the independent 1/8192 Mythic gate fires.
// Intentionally EMPTY for now. Add true chase items here when ready; while
// empty, the Mythic gate produces nothing and falls through.
static const LootPool Pool_Mythic = {
    // { <itemId>, RARITY_MYTHIC },  // add real mythic chase items later
};

// Helper: append src onto dst.
inline void AppendPool(LootPool& dst, const LootPool& src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

// Composed purple pools. Built once, on first access.
// 70 = 60 + its own.  80 = 60 + 70 + its own.
inline const LootPool& GetComposedPurple(LootPoolId id)
{
    static LootPool p60;
    static LootPool p70;
    static LootPool p80;
    static bool built = false;
    if (!built)
    {
        // 60: just its own list
        AppendPool(p60, Pool_Purple60);

        // 70: everything in 60, plus 70's own
        AppendPool(p70, Pool_Purple60);
        AppendPool(p70, Pool_Purple70);

        // 80: everything in 60 and 70, plus 80's own
        AppendPool(p80, Pool_Purple60);
        AppendPool(p80, Pool_Purple70);
        AppendPool(p80, Pool_Purple80);

        built = true;
    }
    switch (id)
    {
        case POOL_PURPLE_70: return p70;
        case POOL_PURPLE_80: return p80;
        case POOL_PURPLE_60:
        default:             return p60;
    }
}

// Single access point. Swap this body for a DB read later without touching
// any calling code.
inline const LootPool& GetLootPool(LootPoolId id)
{
    static const LootPool empty = {};
    switch (id)
    {
        case POOL_WHITE:     return Pool_White;
        case POOL_GREEN:     return Pool_Green;
        case POOL_BLUE:      return Pool_Blue;
        case POOL_PURPLE_60: return GetComposedPurple(POOL_PURPLE_60);
        case POOL_PURPLE_70: return GetComposedPurple(POOL_PURPLE_70);
        case POOL_PURPLE_80: return GetComposedPurple(POOL_PURPLE_80);
        default:             return empty;
    }
}

#endif // MOD_LEVEL_REWARDS_POOLS_H
