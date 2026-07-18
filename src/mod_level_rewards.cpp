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
#include <sstream>
#include <iomanip>
#include "mod_level_rewards_pools.h"

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

// System-mail sender id. Reuse an existing "system" NPC entry you already
// use for auto-generated mail, or define one. 0 is acceptable for MAIL_NORMAL
// system mail in AzerothCore, but a dedicated creature entry is cleaner.
constexpr uint32 REWARD_MAIL_SENDER = 0;

// Inherent chance (percent) that ANY chest ignores its normal reward and
// instead pulls from its tier's themed jackpot pool.
constexpr uint32 JACKPOT_CHANCE = 1;

constexpr uint32 MYTHIC_ODDS       = 8192;   // true 1 in 8192 per chest
constexpr uint32 UNIQUE_ODDS       = 100000; // true 1 in 100000 per chest (dormant)
constexpr uint32 UNIQUE_TOKEN_ITEM = 900500; // placeholder token itemId (create later)

enum ChestTier { 
    TIER_WHITE, 
    TIER_GREEN, 
    TIER_BLUE, 
    TIER_PURPLE_60, 
    TIER_PURPLE_70, 
    TIER_PURPLE_80 
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

// Safely mails a single item to a player. Returns false only if the
// item template is invalid. Never dereferences the player as a sender.
static bool MailRewardItem(Player* player, uint32 itemId, uint32 count,
                           std::string const& subject, std::string const& body)
{
    if (!player)
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return false;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    MailDraft draft(subject, body);

    // Let MailDraft create + persist the item inside THIS transaction.
    if (Item* item = Item::CreateItem(itemId, count, player))
    {
        item->SaveToDB(trans);          // persisted inside the real transaction
        draft.AddItem(item);
    }
    else
    {
        CharacterDatabase.CommitTransaction(trans);
        return false;
    }

    // System sender (GM/creature), NOT the receiving player.
    draft.SendMailTo(trans, MailReceiver(player), MailSender(MAIL_CREATURE, REWARD_MAIL_SENDER, MAIL_STATIONERY_GM));

    CharacterDatabase.CommitTransaction(trans);
    return true;
}

// Builds a client-clickable item hyperlink for chat output.
static std::string BuildItemLink(Player* player, ItemTemplate const* proto)
{
    if (!proto)
        return "";

    // Quality color, e.g. 0xff1eff00 for uncommon. Mask to RGB.
    uint32 color = ItemQualityColors[proto->Quality] & 0x00FFFFFF;

    // Locale-correct name if you have localized item names; Name1 is the default.
    std::string const& name = proto->Name1;

    // playerLevel field scales the tooltip's "heirloom" style display; the
    // player's level is the safe, conventional value to pass.
    uint32 playerLevel = player ? player->GetLevel() : 0;

    std::ostringstream oss;
    oss << "|c" << std::hex << std::setw(8) << std::setfill('0')
        << (0xFF000000 | color) << std::dec
        << "|Hitem:" << proto->ItemId
        << ":0:0:0:0:0:0:0:" << playerLevel
        << "|h[" << name << "]|h|r";

    return oss.str();
}

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

// Rolls one item from a pool using rarity-derived weights, skipping items the
// player's faction can't use. Writes the chosen rarity to outRarity.
static uint32 RollFromPool(LootPoolId poolId, LootRarity& outRarity, Player* player)
{
    const LootPool& pool = GetLootPool(poolId);
    if (pool.empty() || !player)
        return 0;

    TeamId team = player->GetTeamId();
    auto eligible = [team](const PoolItem& pi) -> bool
    {
        if (pi.faction == FACTION_BOTH)     return true;
        if (pi.faction == FACTION_ALLIANCE) return team == TEAM_ALLIANCE;
        return team == TEAM_HORDE;
    };

    // Stage 1: bucket eligible items by rarity, sum band weights of non-empty bands.
    std::vector<uint32> byBand[RARITY_UNIQUE + 1]; // index by LootRarity
    for (const PoolItem& pi : pool)
        if (eligible(pi))
            byBand[pi.rarity].push_back(pi.itemId);

    uint32 totalBandWeight = 0;
    for (uint8 r = 0; r <= RARITY_UNIQUE; ++r)
        if (!byBand[r].empty())
            totalBandWeight += GetBandWeight(static_cast<LootRarity>(r));

    if (totalBandWeight == 0)
        return 0;

    // Pick a band.
    uint32 roll = urand(1, totalBandWeight);
    uint32 acc = 0;
    LootRarity chosen = RARITY_COMMON;
    for (uint8 r = 0; r <= RARITY_UNIQUE; ++r)
    {
        if (byBand[r].empty())
            continue;
        acc += GetBandWeight(static_cast<LootRarity>(r));
        if (roll <= acc)
        {
            chosen = static_cast<LootRarity>(r);
            break;
        }
    }

    // Stage 2: uniform pick within the chosen band.
    const std::vector<uint32>& items = byBand[chosen];
    outRarity = chosen;
    return items[urand(0, items.size() - 1)];
}

// Chance-out-of-100 helper for readability.
static bool Chance(uint32 percent) { return urand(1, 100) <= percent; }

void GenerateChestLoot(Player* player, ChestTier tier)
{
    uint32 rewardedItemId = 0;
    LootRarity rolledRarity = RARITY_COMMON;
    bool bigWin = false;

    // --- LAYER 1: UNIQUE. Independent true-odds roll. Placeholder reward. ---
    if (urand(1, UNIQUE_ODDS) == 1)
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffFF00FF[UNIQUE!]|r You have drawn a ONE-OF-A-KIND reward token! Contact a GM to claim a unique item or title.");
        std::string ann = "|cffFF00FF[SERVER UNIQUE]|r " + GetClassColor(player) + player->GetName() +
            "|cffFFFFFF has drawn a truly UNIQUE reward - the only one that will ever exist!";
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, ann.c_str());

        // Placeholder token if it exists; otherwise nothing is granted (GM handles it).
        if (sObjectMgr->GetItemTemplate(UNIQUE_TOKEN_ITEM))
        {
            ItemPosCountVec dest;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, UNIQUE_TOKEN_ITEM, 1) == EQUIP_ERR_OK)
                player->StoreNewItem(dest, UNIQUE_TOKEN_ITEM, true);
            else
                MailRewardItem(player, UNIQUE_TOKEN_ITEM, 1, "Unique Reward Token",
                    "Your bags were full. Contact a GM to claim your unique reward.");
        }
        return;
    }

    // --- LAYER 2: MYTHIC. Independent true 1/8192. Empty pool = no-op for now. ---
    if (urand(1, MYTHIC_ODDS) == 1)
    {
        LootRarity dummy;
        uint32 mythicItem = RollFromPool(POOL_MYTHIC, dummy, player);
        if (mythicItem != 0)
        {
            rewardedItemId = mythicItem;
            rolledRarity   = RARITY_MYTHIC;
            bigWin         = true;
        }
        // If the Mythic pool is empty, fall through to the normal reward below.
    }

    // --- LAYER 3: 1% jackpot pool (grindable rares). Only if nothing above hit. ---
    if (rewardedItemId == 0 && Chance(JACKPOT_CHANCE))
    {
        LootPoolId poolId; bool hasPool = true;
        switch (tier)
        {
            case TIER_WHITE:     poolId = POOL_WHITE;     break;
            case TIER_GREEN:     poolId = POOL_GREEN;     break;
            case TIER_BLUE:      poolId = POOL_BLUE;      break;
            case TIER_PURPLE_60: poolId = POOL_PURPLE_60; break;
            case TIER_PURPLE_70: poolId = POOL_PURPLE_70; break;
            case TIER_PURPLE_80: poolId = POOL_PURPLE_80; break;
            default:             hasPool = false;         break;
        }
        if (hasPool)
        {
            rewardedItemId = RollFromPool(poolId, rolledRarity, player);
            if (rewardedItemId != 0)
                bigWin = true;
        }
    }

    // --- LAYER 4: normal reward (the common path). Optional per-tier sub-chances. ---
    if (rewardedItemId == 0)
    {
        switch (tier)
        {
            case TIER_WHITE:
                rewardedItemId = Chance(15) ? 4499 /* Huge Brown Sack (12 slot) */
                                            : TOKEN_WHITE_GEAR;
                break;
            case TIER_GREEN:
                rewardedItemId = TOKEN_GREEN_GEAR;
                break;
            case TIER_BLUE:
                rewardedItemId = TOKEN_GREEN_GEAR;
                break;
            case TIER_PURPLE_60:
            case TIER_PURPLE_70:
            case TIER_PURPLE_80:
                rewardedItemId = TOKEN_BLUE_GEAR;
                break;
        }
    }

    if (rewardedItemId == 0)
        return;

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

        std::string link = BuildItemLink(player, itemTemplate);

        if (bigWin)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFFFF00[JACKPOT!]|r: Cosmic luck! You opened a legendary upgrade: {}", link);

            std::string odds = "1/" + std::to_string(100 / JACKPOT_CHANCE); // e.g. 1 -> "1/100"
            std::string announceStr = "|cffFF0000[SERVER JACKPOT]|r " + GetClassColor(player) + player->GetName() +
                "|cffFFFFFF has beaten the |cff00FF00" + odds + "|cffFFFFFF odds from a Reward Chest and drawn: " + link + "!";
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announceStr.c_str());
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00FF00[Reward Chest]|r: You opened the chest and looted: {}", link);
        }
    }
    else
    {
        std::string link = BuildItemLink(player, itemTemplate);

        bool mailed = MailRewardItem(player, rewardedItemId, 1,
            "Reward Chest Loot",
            "Your bags were full when opening the chest, so we securely mailed your loot.");

        if (!mailed)
        {
            LOG_ERROR("module", "mod_level_rewards: Failed to mail loot {} to {}.", rewardedItemId, player->GetName());
            ChatHandler(player->GetSession()).PSendSysMessage("The reward could not be delivered. Please contact a GM.");
            return;
        }

        if (bigWin)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFF0000[JACKPOT!]|r: Bags full! Your legendary reward {} was sent to your mailbox.", link);

            std::string announceStr = "|cffFF0000[SERVER JACKPOT]|r " + GetClassColor(player) + player->GetName() +
                "|cffFFFFFF has beaten the |cff00FF001/1000|cffFFFFFF odds and drawn: " + link + "! (Sent to mail)";
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announceStr.c_str());
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFF0000[Reward Chest]|r: Bags full! {} was sent to your mailbox.", link);
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
        if (MailRewardItem(player, chestItemId, 1,
                "Level Up Chest!",
                "Congratulations on your new level! Your bags were full, so we mailed your Reward Chest."))
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFF0000[Level Reward]|r: Your bags are full! Your Reward Chest has been sent to your mailbox.");
        }
        else
        {
            LOG_ERROR("module", "mod_level_rewards: Failed to mail chest {} to {}.", chestItemId, player->GetName());
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
                return "您获得了一个奖励宝箱！";
            case LOCALE_esES:
            case LOCALE_esMX:
                return "¡Has recibido un cofre de recompensa!";
            default:
                return "You have received a Reward Chest!";
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
    ModLevelRewardsWorldScript() : WorldScript("ModLevelRewardsWorldScript", { 
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP 
    }) { }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        sConfigMgr->LoadModulesConfigs();
        sRewardsConfig.motdMessageId        = sConfigMgr->GetOption<uint32>("ModLevelRewards.Motd.String.ID", 60000);
        sRewardsConfig.motdAnnounce         = sConfigMgr->GetOption<bool>("ModLevelRewards.Motd.Announce", true);
        sRewardsConfig.enableModule         = sConfigMgr->GetOption<bool>("ModLevelRewards.Enable", true);
        sRewardsConfig.enableGlobalAnnounce = sConfigMgr->GetOption<bool>("ModLevelRewards.GlobalAnnounce.Enable", true);
        sRewardsConfig.enableChestRewards   = sConfigMgr->GetOption<bool>("ModLevelRewards.ChestRewards.Enable", true);
    }

    void OnStartup() override
    {
        // The database is completely connected and ready here. Safe to query!
        PopulateLootGenerationCache();
    }
};

void AddLevelRewardsScripts()
{
    new ModLevelRewardsMOTD();
    new ModLevelRewardsLevelUp();
    new ModLevelRewardsWorldScript();
    new Script_RewardChestItem();
}