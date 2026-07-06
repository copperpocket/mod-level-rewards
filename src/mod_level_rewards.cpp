#include "Configuration/Config.h"
#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSessionMgr.h"
#include "AccountMgr.h"
#include "Mail.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Log.h"
#include <vector>
#include <string>
#include <unordered_map>

// --- CUSTOM CHEST ITEM IDs ---
constexpr uint32 ITEM_CHEST_WHITE    = 900100;
constexpr uint32 ITEM_CHEST_GREEN    = 900101;
constexpr uint32 ITEM_CHEST_BLUE     = 900102;
constexpr uint32 ITEM_CHEST_PURPLE60 = 900103;
constexpr uint32 ITEM_CHEST_PURPLE70 = 900104;
constexpr uint32 ITEM_CHEST_PURPLE80 = 900105;

constexpr uint32 TOKEN_WHITE_GEAR = 900001;
constexpr uint32 TOKEN_GREEN_GEAR = 900002;
constexpr uint32 TOKEN_BLUE_GEAR  = 900003;

enum ChestTier { 
    TIER_WHITE, 
    TIER_GREEN, 
    TIER_BLUE, 
    TIER_PURPLE_60, 
    TIER_PURPLE_70, 
    TIER_PURPLE_80 
};

struct WeightedItem {
    uint32 itemId;
    uint32 weight;
};

struct ModLevelRewardsConfig
{
    uint32 motdMessageId;
    bool   motdAnnounce;
    bool   enableModule;
    bool   enableGlobalAnnounce;
    bool   enableChestRewards;
};

static ModLevelRewardsConfig sRewardsConfig;

// Thread-safe memory cache for rolling level-appropriate base gear
// Composite Key: ((Class << 16) | (Quality << 8) | RequiredLevel) -> list of items
static std::unordered_map<uint32, std::vector<uint32>> sCachedGearPools;

std::string GetClassColor(Player* player) {
    switch (player->getClass()) {
        case CLASS_WARRIOR: return "|cffC69B6D";
        case CLASS_PALADIN: return "|cffF48CBA";
        case CLASS_HUNTER:  return "|cffAAD372";
        case CLASS_ROGUE:   return "|cffFFF468";
        case CLASS_PRIEST:  return "|cffFFFFFF";
        case CLASS_DEATH_KNIGHT: return "|cffC41E3A";
        case CLASS_SHAMAN:  return "|cff0070DD";
        case CLASS_MAGE:    return "|cff3FC7EB";
        case CLASS_WARLOCK: return "|cff8788EE";
        case CLASS_DRUID:   return "|cffFF7C0A";
        default:            return "|cff636363";
    }
}

uint32 GetOptimalArmorSubclass(Player* player) {
    uint8 pClass = player->getClass();
    uint8 pLevel = player->GetLevel();

    switch (pClass) {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            return (pLevel >= 40) ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return (pLevel >= 40) ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        default:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

uint32 GetWeaponSubclassForClass(Player* player) {
    uint8 pClass = player->getClass();
    switch (pClass) {
        case CLASS_WARRIOR:
        case CLASS_PALADIN: {
            uint32 weaponPool[] = { 0, 1, 7, 6 }; // 1H Sword, 1H Axe, 2H Sword, 2H Axe
            return weaponPool[urand(0, 3)];
        }
        case CLASS_ROGUE:
            return urand(0, 1) ? 15 : 0; // Dagger or 1H Sword
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return 10; // Staff
        case CLASS_HUNTER:
            return 2;  // Bow
        default:
            return 5;  // Mace
    }
}

uint32 GetShieldSubclass(Player* player) {
    // Only classes that can actually use shields
    if (player->getClass() == CLASS_WARRIOR || player->getClass() == CLASS_PALADIN || player->getClass() == CLASS_SHAMAN)
        return 6; // Armor: Shield
    return 0;     // Return something invalid/safe if they can't
}

// --- DISTINCT TIER-BASED JACKPOT POOLS (1/100 Odds) ---
// All Jackpot Pools are weighted out of 1000 to handle the large variety of chase items.
const std::vector<WeightedItem>& GetJackpotPool(ChestTier tier) {

    // White Jackpot (Total Weight: 1000) 
    static const std::vector<WeightedItem> WhiteJackpot = {
        { 14156, 100 }  // 100% - Bottomless Bag (18 Slot)
    };

    // Green Jackpot (Total Weight: 1000)
    static const std::vector<WeightedItem> GreenJackpot = {
        { 14156, 510 }, // 51.0% - Bottomless Bag (18 Slot)
        { 5462,  35 },  //  3.5% - Dartol's Rod of Transformation (Vanilla Quest Blue)
        { 40110, 35 },  //  3.5% - Haunted Memento (WotLK Scourge Event Blue)
        { 32566, 35 },  //  3.5% - Picnic Basket (TBC TCG Uncommon)
        { 34499, 35 },  //  3.5% - Paper Flying Machine Kit (TBC MofL TCG Uncommon)
        { 45063, 35 },  //  3.5% - Foam Sword Rack (WotLK BoG TCG Uncommon)
        { 46780, 35 },  //  3.5% - Ogre Pinata (WotLK FoH TCG Uncommon)
        { 23713, 35 },  //  3.5% - Hippogryph Hatchling (Vanilla TCG Companion)
        { 32588, 35 },  //  3.5% - Banana Charm (TBC TCG Companion)
        { 34493, 35 },  //  3.5% - Dragon Kite (TBC TCG Companion)
        { 20371, 35 },  //  3.5% - Blue Murloc Egg (Vanilla Blizzcon Murky)
        { 30360, 35 },  //  3.5% - Lurky's Egg (TBC CE Europe Variant)
        { 46802, 35 },  //  3.5% - Heavy Murloc Egg (WotLK Blizzcon Grunty)
        { 49362, 35 },  //  3.5% - Onyxian Whelpling (WoW 5th Anniversary)
        { 44819, 35 }   //  3.5% - Baby Blizzard Bear (WoW 4th Anniversary)
    };

    // Blue Jackpot (Total Weight: 1000)
    static const std::vector<WeightedItem> BlueJackpot = {
        { 14156, 410 }, // 41.0% - Bottomless Bag (18 Slot)
        { 8630,  50 },  //  5.0% - Reins of the Bengal Tiger (Alpha)
        { 8633,  50 },  //  5.0% - Reins of the Leopard (Alpha)
        { 19160, 35 },  //  3.5% - Contest Winner's Tabard (Vanilla Blue)
        { 28788, 35 },  //  3.5% - Tabard of the Protector (TBC Event Blue)
        { 36941, 35 },  //  3.5% - Competitor's Tabard (TBC Olympic Event Blue)
        { 40601, 35 },  //  3.5% - Argent Dawn Banner (WotLK Event Blue)
        { 23705, 35 },  //  3.5% - Tabard of Flame (Vanilla TCG Purple)
        { 32542, 35 },  //  3.5% - Imp in a Ball (TBC TCG Rare)
        { 33219, 35 },  //  3.5% - Goblin Gumbo Kettle (TBC FoO TCG Uncommon)
        { 33223, 35 },  //  3.5% - Fishing Chair (TBC FoO TCG Uncommon)
        { 35227, 35 },  //  3.5% - Goblin Weather Machine - Prototype (TBC SotB TCG Uncommon)
        { 34492, 35 },  //  3.5% - Rocket Chicken (TBC MofL TCG Rare)
        { 38301, 35 },  //  3.5% - D.I.S.C.O. (TBC HfI TCG Uncommon)
        { 38578, 35 },  //  3.5% - The Flag of Ownership (TBC DoW TCG Uncommon)
        { 54452, 35 },  //  3.5% - Ethereal Portal (WotLK Icecrown TCG Uncommon)
        { 23720, 35 }   //  3.5% - Riding Turtle (Vanilla TCG Sea Mount)
    };

    // Level 60 Purple Jackpot (Total Weight: 1000)
    static const std::vector<WeightedItem> Purple60Jackpot = {
        { 14156, 400 }, // 40.0% - Bottomless Bag (18 Slot)
        { 13325, 50 },  //  5.0% - Fluorescent Green Mechanostrider
        { 22999, 45 },  //  4.5% - Tabard of the Argent Dawn (Vanilla Scourge Invasion)
        { 13584, 45 },  //  4.5% - Diablo Stone (Vanilla Collector's Edition)
        { 13582, 45 },  //  4.5% - Zergling Leash (Vanilla Collector's Edition)
        { 13583, 45 },  //  4.5% - Panda Collar (Vanilla Collector's Edition)
        { 12353, 45 },  //  4.5% - White Stallion Bridle (Unarmored Epic Mount)
        { 13327, 45 },  //  4.5% - Icy Blue Mechanostrider Mod A (Unarmored Epic Mount)
        { 12354, 45 },  //  4.5% - Palomino Bridle (Unarmored Epic Mount)
        { 8586,  45 },  //  4.5% - Whistle of the Mottled Red Raptor (Unarmored Epic Mount)
        { 13317, 45 },  //  4.5% - Whistle of the Ivory Raptor (Unarmored Epic Mount)
        { 15293, 45 },  //  4.5% - Teal Kodo (Unarmored Epic Mount)
        { 13328, 45 },  //  4.5% - Black Ram (Unarmored Epic Mount)
        { 13329, 55 }   //  5.5% - Frost Ram (Unarmored Epic Mount)
    };

    // Level 70 Purple Jackpot (Total Weight: 1000)
    static const std::vector<WeightedItem> Purple70Jackpot = {
        { 21876, 480 }, // 48.0% - Primal Mooncloth Bag (20 Slot TBC Core Bag)
        { 43599, 50 },  //  5.0% - Big Blizzard Bear
        { 49284, 30 },  //  3.0% - Reins of the Swift Spectral Tiger
        { 25535, 70 },  //  7.0% - Netherwhelp's Collar (TBC Collector's Edition)
        { 34518, 70 },  //  7.0% - Golden Pig Coin (Regional Event)
        { 34519, 70 },  //  7.0% - Silver Pig Coin (Regional Event)
        { 38050, 70 },  //  7.0% - Soul-Trader Beacon (TBC TCG Ultra-Rare Pet)
        { 37297, 70 },  //  7.0% - Gold Medallion (Spirit of Competition Event)
        { 49282, 70 },  //  7.0% - Big Battle Bear (Drums of War TCG Rare)
        { 49290, 20 }   //  2.0% - Magic Rooster Egg
    };

    // Level 80 Purple Jackpot (Total Weight: 1000)
    static const std::vector<WeightedItem> Purple80Jackpot = {
        { 41599, 420 }, // 42.0% - Glacial Bag (22 Slot WotLK Core Bag)
        { 49286, 20 },  //  2.0% - X-51 Nether-Rocket X-TREME
        { 46894, 40 },  //  4.0% - Enchanted Jade (Event Item)
        { 49664, 40 },  //  4.0% - Enchanted Purple Jade (Event Item)
        { 39656, 40 },  //  4.0% - Tyrael's Hilt (Worldwide Invitational Paris 2008)
        { 45037, 40 },  //  4.0% - Epic Purple Shirt (Blood of Gladiators TCG Rare)
        { 39286, 40 },  //  4.0% - Frosty's Collar (WotLK Collector's Edition)
        { 56806, 40 },  //  4.0% - Mini-Thor (StarCraft II Collector's Edition tie-in)
        { 49343, 40 },  //  4.0% - Spectral Tiger Cub (Scourgewar TCG Ultra-Rare Companion)
        { 49287, 40 },  //  4.0% - Tuskarr Kite (Scourgewar TCG Rare Companion)
        { 37719, 40 },  //  4.0% - Swift Zhevra (Recruit-A-Friend Classic)
        { 54860, 40 },  //  4.0% - X-53 Touring Rocket (Recruit-A-Friend WotLK)
        { 44164, 40 },  //  4.0% - Reins of the Black Proto-Drake (Glory of the Raider 10)
        { 44175, 40 },  //  4.0% - Reins of the Plagued Proto-Drake (Glory of the Raider 25)
        { 54069, 40 },  //  4.0% - Blazing Hippogryph (Wrathgate TCG Rare Mount)
        { 54068, 40 }   //  4.0% - Wooly White Rhino (Icecrown TCG Rare Mount)
    };

    switch(tier) {
        case TIER_GREEN:     return GreenJackpot;
        case TIER_BLUE:      return BlueJackpot;
        case TIER_PURPLE_60: return Purple60Jackpot;
        case TIER_PURPLE_70: return Purple70Jackpot;
        case TIER_PURPLE_80: return Purple80Jackpot;
        case TIER_WHITE:
        default:             return WhiteJackpot;
    }
}

// --- STANDARD LOOT POOLS ---
// All standard Pools are weighted out of 100
const std::vector<WeightedItem>& GetPool(ChestTier tier) {

    // White Pool (Total Weight: 100)
    static const std::vector<WeightedItem> WhitePool = {
        { TOKEN_WHITE_GEAR, 85 },  // 85.0% - Level-appropriate white gear
        { 5573, 10 },              // 10.0% - Green Leather Bag/8
        { 5576, 5 }                //  5.0% - Large Brown Sack/10
    };

    // Green Pool (Total Weight: 100)
    static const std::vector<WeightedItem> GreenPool = {
        { TOKEN_GREEN_GEAR, 85 },  // 85.0% - Level-appropriate green gear
        { 3914, 10 },              // 10.0% - Journeyman's Backpack/14
        { 5576, 5 }                //  5.0% - Large Brown Sack/10
    };

    // Blue Pool (Total Weight: 100)
    static const std::vector<WeightedItem> BluePool = {
        { TOKEN_GREEN_GEAR, 85 },  // Level-appropriate green gear
        { 4500, 15 },              // Traveler's Backpack/16 (White)
    };

    // Level 60 Epic Pool
    static const std::vector<WeightedItem> PurplePool60 = {
        { TOKEN_BLUE_GEAR, 100 },   // Level-appropriate blue gear
    };

    // Level 70 Epic Pool
    static const std::vector<WeightedItem> PurplePool70 = {
        { TOKEN_BLUE_GEAR, 100 },   // Level-appropriate blue gear
    };

    // Level 80 Epic Pool
    static const std::vector<WeightedItem> PurplePool80 = {
        { TOKEN_BLUE_GEAR, 100 },   // Level-appropriate blue gear
    };

    switch(tier) {
        case TIER_GREEN:     return GreenPool;
        case TIER_BLUE:      return BluePool;
        case TIER_PURPLE_60: return PurplePool60;
        case TIER_PURPLE_70: return PurplePool70;
        case TIER_PURPLE_80: return PurplePool80;
        case TIER_WHITE:
        default:             return WhitePool;
    }
}

// Memory caching built to completely bypass running sync SQL queries on the main thread during item use hooks.
void PopulateLootGenerationCache()
{
    sCachedGearPools.clear();
    uint32 itemsLoaded = 0;

    QueryResult result = WorldDatabase.Query(
        "SELECT entry, class, subclass, Quality, RequiredLevel, AllowableClass FROM item_template "
        "WHERE ((class = 2 AND subclass IN (0,1,2,6,7,10,15)) OR (class = 4 AND subclass IN (1,2,3,4,6))) "
        "AND Quality IN (1, 2, 3) AND RequiredLevel <= 80 AND name NOT LIKE 'Test%'"
    );

    if (!result)
    {
        LOG_ERROR("server.loading", "PopulateLootGenerationCache: Zero template rows detected! Adaptive chest gear rolling will fail.");
        return;
    }

    do {
        Field* fields = result->Fetch();
        uint32 entry = fields[0].Get<uint32>();
        uint32 iClass = fields[1].Get<uint32>();
        uint32 subClass = fields[2].Get<uint32>();
        uint32 quality = fields[3].Get<uint32>();
        uint32 reqLevel = fields[4].Get<uint32>();
        int32 allowClass = fields[5].Get<int32>();

        // Build item descriptors to identify which character class pools this item belongs to
        for (uint8 c = 1; c <= MAX_CLASSES; ++c)
        {
            uint32 classMask = (1 << (c - 1));
            if (allowClass != -1 && !(allowClass & classMask))
                continue;

            // Generate precise indexing token
            uint32 cacheToken = (static_cast<uint32>(c) << 16) | (quality << 8) | reqLevel;
            sCachedGearPools[cacheToken].push_back(entry);
            itemsLoaded++;
        }
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} system mappings into ModLevelRewards dynamic item caches.", itemsLoaded);
}

void GenerateChestLoot(Player* player, ChestTier tier)
{
    bool hitJackpot = (urand(1, 100) == 1);
    const std::vector<WeightedItem>& pool = hitJackpot ? GetJackpotPool(tier) : GetPool(tier);

    if (pool.empty()) return;

    uint32 totalWeight = 0;
    for (const auto& item : pool)
        totalWeight += item.weight;

    uint32 roll = urand(1, totalWeight);
    uint32 currentWeight = 0;
    uint32 rewardedItemId = 0;

    for (const auto& item : pool)
    {
        currentWeight += item.weight;
        if (roll <= currentWeight)
        {
            rewardedItemId = item.itemId;
            break;
        }
    }

    if (rewardedItemId == 0) return;

    if (rewardedItemId == TOKEN_WHITE_GEAR || rewardedItemId == TOKEN_GREEN_GEAR || rewardedItemId == TOKEN_BLUE_GEAR) 
    {
        uint32 targetQuality = (rewardedItemId == TOKEN_WHITE_GEAR) ? ITEM_QUALITY_NORMAL : 
                              (rewardedItemId == TOKEN_GREEN_GEAR) ? ITEM_QUALITY_UNCOMMON : ITEM_QUALITY_RARE;

        uint32 pLevel = player->GetLevel();
        uint32 minLevel = (pLevel > 5) ? pLevel - 5 : 1;
        uint32 maxLevel = pLevel;
        uint8  pClass = player->getClass();

        uint32 weaponSubclass = GetWeaponSubclassForClass(player);
        uint32 armorSubclass = GetOptimalArmorSubclass(player);
        uint32 shieldSubclass = GetShieldSubclass(player);

        std::vector<uint32> validFilteredItems;

        // Query the thread-safe memory lookup cache across allowable target level brackets
        for (uint32 lvl = minLevel; lvl <= maxLevel; ++lvl)
        {
            uint32 cacheToken = (static_cast<uint32>(pClass) << 16) | (targetQuality << 8) | lvl;
            auto it = sCachedGearPools.find(cacheToken);
            if (it != sCachedGearPools.end())
            {
                for (uint32 entry : it->second)
                {
                    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                    if (!proto) continue;

                    if ((proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == weaponSubclass) ||
                        (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == armorSubclass) ||
                        (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == shieldSubclass))
                    {
                        validFilteredItems.push_back(entry);
                    }
                }
            }
        }

        if (!validFilteredItems.empty())
        {
            rewardedItemId = validFilteredItems[urand(0, validFilteredItems.size() - 1)];
        } 
        else 
        {
            LOG_ERROR("server.loading", "GenerateChestLoot: Failed to roll item from cache for {} (Lvl: {}, Class: {}). Check filters.", 
                      player->GetName(), player->GetLevel(), (uint32)pClass);
            ChatHandler(player->GetSession()).PSendSysMessage("The chest was empty (No items found matching your specialization layout).");
            return;
        }
    }

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(rewardedItemId);
    if (!itemTemplate) return;

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, rewardedItemId, 1);
    
    if (msg == EQUIP_ERR_OK)
    {
        player->StoreNewItem(dest, rewardedItemId, true);
        if (hitJackpot)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffFFFF00[JACKPOT!]|r: Cosmic luck! You opened a legendary tier-exclusive upgrade!");
            std::string announceStr = "|cffFF0000[SERVER JACKPOT]|r " + GetClassColor(player) + player->GetName() + 
                                      "|cffFFFFFF has beaten the |cff00FF001/1000|cffFFFFFF odds from a Reward Chest and drawn: " + itemTemplate->Name1 + "!";
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announceStr.c_str());
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cff00FF00[Reward Chest]|r: You successfully opened the chest and looted an item!");
        }
    }
    else
    {
        Item* item = Item::CreateItem(rewardedItemId, 1, player);
        if (item)
        {
            item->SaveToDB(nullptr);
            MailDraft("Reward Chest Loot", "Your bags were full when opening the chest, so we securely mailed your loot.")
                .AddItem(item)
                .SendMailTo(nullptr, MailReceiver(player), MailSender(player));
            
            if (hitJackpot)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffFF0000[JACKPOT!]|r: Your bags are full! Your legendary reward has been sent to your mailbox.");
                std::string announceStr = "|cffFF0000[SERVER JACKPOT]|r " + GetClassColor(player) + player->GetName() + 
                                          "|cffFFFFFF has beaten the |cff00FF001/1000|cffFFFFFF odds and drawn: " + itemTemplate->Name1 + "! (Sent to mail)";
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announceStr.c_str());
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffFF0000[Reward Chest]|r: Your bags are full! The chest loot has been sent to your mailbox.");
            }
        }
    }
}

void GiveBoERewardChest(Player* player, ChestTier tier)
{
    uint32 chestItemId = 0;
    switch(tier) {
        case TIER_WHITE:     chestItemId = ITEM_CHEST_WHITE;    break;
        case TIER_GREEN:     chestItemId = ITEM_CHEST_GREEN;    break;
        case TIER_BLUE:      chestItemId = ITEM_CHEST_BLUE;     break;
        case TIER_PURPLE_60: chestItemId = ITEM_CHEST_PURPLE60; break;
        case TIER_PURPLE_70: chestItemId = ITEM_CHEST_PURPLE70; break;
        case TIER_PURPLE_80: chestItemId = ITEM_CHEST_PURPLE80; break;
    }

    if (chestItemId == 0) return;

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, chestItemId, 1);
    
    if (msg == EQUIP_ERR_OK)
    {
        player->StoreNewItem(dest, chestItemId, true);
    }
    else
    {
        Item* item = Item::CreateItem(chestItemId, 1, player);
        if (item)
        {
            item->SaveToDB(nullptr);
            MailDraft("Level Up Chest!", "Congratulations on your new level! Your bags were full, so we mailed your Reward Chest.")
                .AddItem(item)
                .SendMailTo(nullptr, MailReceiver(player), MailSender(player));
            
            ChatHandler(player->GetSession()).PSendSysMessage("|cffFF0000[Level Reward]|r: Your bags are full! Your Reward Chest has been sent to your mailbox.");
        }
    }
}

uint32 GiveDatabaseRewards(Player* player)
{
    QueryResult result = WorldDatabase.Query("SELECT * FROM `mod_level_rewards_items` WHERE `level`={} AND (`race`={} OR `race`=0) AND (`class`={} OR `class`=0)", player->GetLevel(), player->getRace(), player->getClass());
    uint32 money = 0;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            if (fields[1].Get<uint32>() > 0)
            {
                player->ModifyMoney(fields[1].Get<uint32>() * GOLD);
                money += fields[1].Get<uint32>() * GOLD;
            }
            if (fields[2].Get<uint32>() > 0)
            {
                if (fields[3].Get<bool>() == false)
                    player->CastSpell(player, fields[2].Get<uint32>());
                else
                    player->learnSpell(fields[2].Get<uint32>());
            }
            if (fields[4].Get<uint32>() > 0)
                player->AddItem(fields[4].Get<uint32>(), 1);
            if (fields[5].Get<uint32>() > 0)
                player->AddItem(fields[5].Get<uint32>(), 1);
        } while (result->NextRow());
    }
    return money;
}

class Script_RewardChestItem : public ItemScript
{
public:
    Script_RewardChestItem() : ItemScript("Script_RewardChestItem") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        ChestTier tier;
        switch (item->GetEntry())
        {
            case ITEM_CHEST_WHITE:     tier = TIER_WHITE;     break;
            case ITEM_CHEST_GREEN:     tier = TIER_GREEN;     break;
            case ITEM_CHEST_BLUE:      tier = TIER_BLUE;      break;
            case ITEM_CHEST_PURPLE60:  tier = TIER_PURPLE_60; break;
            case ITEM_CHEST_PURPLE70:  tier = TIER_PURPLE_70; break;
            case ITEM_CHEST_PURPLE80:  tier = TIER_PURPLE_80; break;
            default: return false;
        }

        GenerateChestLoot(player, tier);
        player->DestroyItemCount(item->GetEntry(), 1, true);
        return true; 
    }
};

class ModLevelRewardsMOTD : public PlayerScript
{
public:
    ModLevelRewardsMOTD() : PlayerScript("ModLevelRewardsMOTD", { PLAYERHOOK_ON_LOGIN }) {}

    void OnPlayerLogin(Player* player) override
    {
        if (sRewardsConfig.motdAnnounce)
            ChatHandler(player->GetSession()).SendSysMessage(sRewardsConfig.motdMessageId);
    }
};

std::string GetLocalizedMessage(Player* player, uint8 level, bool isGlobalAnnounce) {
    auto locale = player->GetSession()->GetSessionDbLocaleIndex();
    std::string name = player->GetName();

    if (isGlobalAnnounce) {
        std::string color = GetClassColor(player);
        switch (locale) {
            case LOCALE_zhCN:
            case LOCALE_zhTW:
                return "|cffFFFFFF[ |cffFF0000恭|cffFFA500喜|cffFFFF00你|cff00FF00升|cff00FFFF级|cff6A5ACD啦|cffFF00FF! |cffFFFFFF] : |cff4CFF00 " + name + " |cffFFFFFF已达到 |cff4CFF00" + std::to_string(level) + " 级|cffFFFFFF!";
            case LOCALE_esES:
            case LOCALE_esMX:
                return "|cffFFFFFF[ |cffFF0000F|cffFFA500E|cffFFFF00L|cff00FF00I|cff00FFFFC|cff6A5ACDI|cffFF00FFT|cff98FB98A|cff00FF00C|cff00FFFFI|cffFF0000O|cff00FF00N|cff00FFFFE|cffFF00FFS|cffFF0000! |cffFFFFFF] : |cff4CFF00 " + name + " |cffFFFFFFha alcanzado |cff4CFF00el nivel " + std::to_string(level) + "|cffFFFFFF!";
            default:
                return "|cffFFFFFF[DING!] " + color + name + "|cffFFFFFF has reached |cff4CFF00Level " + std::to_string(level) + "|cffFFFFFF!";
        }
    } else {
        switch (locale) {
            case LOCALE_zhCN:
            case LOCALE_zhTW:
                return "恭喜达到等级 " + std::to_string(level) + "，" + name + "！您获得了一个奖励宝箱！";
            case LOCALE_esES:
            case LOCALE_esMX:
                return "¡Felicidades por el nivel " + std::to_string(level) + " " + name + "! ¡Has recibido un cofre de recompensa!";
            default:
                return "Congrats on Level " + std::to_string(level) + " " + name + "! You have received a Reward Chest!";
        }
    }
}

class ModLevelRewardsLevelUp : public PlayerScript
{
public:
    ModLevelRewardsLevelUp() : PlayerScript("ModLevelRewardsLevelUp", { PLAYERHOOK_ON_LEVEL_CHANGED }) { }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!sRewardsConfig.enableModule) return;
        if (!player->GetSession()) return;

        std::string accountName;
        if (AccountMgr::GetName(player->GetSession()->GetAccountId(), accountName))
        {
            if (accountName.find("RNDBOT") != std::string::npos) return;
        }

        uint8 level = player->GetLevel();

        if (sRewardsConfig.enableGlobalAnnounce && oldLevel < level)
        {
            std::string msg = GetLocalizedMessage(player, level, true);
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, msg.c_str());
        }

        bool chestAwarded = false;
        if (sRewardsConfig.enableChestRewards && oldLevel < level)
        {
            ChestTier tier = TIER_WHITE;
            bool grantChest = false;

            if (level >= 2 && level <= 18 && level % 2 == 0) {
                grantChest = true; tier = TIER_WHITE;
            } 
            else if (level >= 20 && level <= 38 && level % 2 == 0) {
                grantChest = true; tier = TIER_GREEN;
            } 
            else if (level >= 40 && level <= 58 && level % 2 == 0) {
                grantChest = true; tier = TIER_BLUE;
            } 
            else if (level == 60) {
                grantChest = true; tier = TIER_PURPLE_60;
            }
            else if (level == 70) {
                grantChest = true; tier = TIER_PURPLE_70;
            }
            else if (level == 80) {
                grantChest = true; tier = TIER_PURPLE_80;
            }

            if (grantChest) {
                GiveBoERewardChest(player, tier);
                chestAwarded = true;
            }
        }

        uint32 money = 0;
        if (oldLevel < level)
            money = GiveDatabaseRewards(player);

        if (money > 0 || chestAwarded)
        {
            if (auto* session = player->GetSession())
            {
                std::string msg2 = GetLocalizedMessage(player, level, false);
                ChatHandler(session).SendNotification(msg2.c_str());
            }
        }
    }
};

class ModLevelRewardsWorldScript : public WorldScript
{
public:
    ModLevelRewardsWorldScript() : WorldScript("ModLevelRewardsWorldScript", { WORLDHOOK_ON_BEFORE_CONFIG_LOAD }) { }

    void OnBeforeConfigLoad(bool reload) override
    {
        sConfigMgr->LoadModulesConfigs();
        sRewardsConfig.motdMessageId        = sConfigMgr->GetOption<uint32>("ModLevelRewards.Motd.String.ID", 60000);
        sRewardsConfig.motdAnnounce         = sConfigMgr->GetOption<bool>("ModLevelRewards.Motd.Announce", true);
        sRewardsConfig.enableModule         = sConfigMgr->GetOption<bool>("ModLevelRewards.Enable", true);
        sRewardsConfig.enableGlobalAnnounce = sConfigMgr->GetOption<bool>("ModLevelRewards.GlobalAnnounce.Enable", true);
        sRewardsConfig.enableChestRewards   = sConfigMgr->GetOption<bool>("ModLevelRewards.ChestRewards.Enable", true);

        // Load or refresh the dynamic adaptive pool compilation
        PopulateLootGenerationCache();
    }
};

void AddLevelRewardsScripts()
{
    new ModLevelRewardsMOTD();
    new ModLevelRewardsLevelUp();
    new ModLevelRewardsWorldScript();
    new Script_RewardChestItem();
    PopulateLootGenerationCache();
}