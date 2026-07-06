# mod-level-rewards

A modular, database-driven reward system for AzerothCore. This module rewards players upon reaching specific level milestones with a combination of automated gold, spells, and custom "Reward Chests" that provide tiered, weighted loot.

## Features
* **Tiered Reward System:** Automatically grants different tiers of Reward Chests (White, Green, Blue, Purple) based on level brackets.
* **Smart Loot Generation:** Chest loot is generated dynamically based on player class, armor proficiency, and level range.
* **Weighted Jackpot Pools:** Includes a 1/1000 chance for legendary/TCG-tier items, fully configurable.
* **Localization Support:** Easily extensible support for multiple languages in both global announcements and personal notifications.
* **Inventory Handling:** Integrated mail fallback system for when player bags are full.
* **Bot-Proofing:** Automatic filtering to ignore RNDBOT-style accounts.

## Installation
1. Place the `mod-level-rewards` folder into your `modules` directory.
2. Run `cmake` to configure your server files.
3. Apply the SQL files (found in `sql/`) to your `acore_world` database.
4. Recompile your server.

## Database Structure

### `mod_level_rewards_items`
This table defines the static rewards (gold/spells) granted at specific milestones.

| Column | Type | Description |
| :--- | :--- | :--- |
| `level` | `tinyint` | The level at which the reward is triggered. |
| `money` | `int` | Bonus gold (multiplied by GOLD constant). |
| `spell` | `int` | Spell ID to cast or learn. |
| `learn` | `boolean` | If true, player learns the spell; if false, spell is cast. |
| `itemId1` | `int` | Optional primary item ID reward. |
| `itemId2` | `int` | Optional secondary item ID reward. |
| `race`/`class`| `tinyint` | Filter by race/class (0 for all). |

## Configuration
You can configure module behavior via your `worldserver.conf`:

* `ModLevelRewards.Enable` (default: 1) - Toggles the module system.
* `ModLevelRewards.Motd.Announce` (default: 1) - Toggles global startup announcement.
* `ModLevelRewards.GlobalAnnounce.Enable` (default: 1) - Toggles the level-up "DING!" announcement.
* `ModLevelRewards.ChestRewards.Enable` (default: 1) - Toggles the distribution of Reward Chests.
* `ModLevelRewards.Motd.String.ID` (default: 60000) - The acore_string ID for the startup announcement.

## Customization
* **Loot Tables:** To modify the drop rates or contents of the Reward Chests, edit `src/mod_level_rewards.cpp` and update the `WeightedItem` vectors in `GetJackpotPool` and `GetPool`.
* **Adding Items:** Ensure any custom `DisplayInfoID` mappings are applied to your `Item.dbc` if you add new item models.

## License
This module is released under the GNU AGPL v3 license, consistent with the AzerothCore project.