-- Remove old versions of the chests
DELETE FROM `item_template` WHERE `entry` BETWEEN 900100 AND 900105;

-- Insert the working chests with Flags = 0 (bypasses item_loot_template) 
-- and SpellID_1 = 48356 (forces the WoW client to allow right-clicking)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `AllowableClass`, `AllowableRace`, `RequiredLevel`, `SpellID_1`, `SpellTrigger_1`, `Description`, `Material`, `ItemLevel`, `stackable`, `ScriptName`) VALUES
(900100, 0, 0, 'Common Reward Chest', 31031, 1, 0, 128, -1, -1, 1, 48356, 0, 'Right-click to claim your rewards!', -1, 1, 20, 'Script_RewardChestItem'),
(900101, 0, 0, 'Uncommon Reward Chest', 31031, 2, 0, 128, -1, -1, 20, 48356, 0, 'Right-click to claim your rewards!', -1, 20, 20, 'Script_RewardChestItem'),
(900102, 0, 0, 'Rare Reward Chest', 31031, 3, 0, 128, -1, -1, 40, 48356, 0, 'Right-click to claim your rewards!', -1, 40, 20, 'Script_RewardChestItem'),
(900103, 0, 0, 'Epic Reward Chest (60)', 31031, 4, 0, 128, -1, -1, 60, 48356, 0, 'Right-click to claim your rewards!', -1, 60, 20, 'Script_RewardChestItem'),
(900104, 0, 0, 'Epic Reward Chest (70)', 31031, 4, 0, 128, -1, -1, 70, 48356, 0, 'Right-click to claim your rewards!', -1, 70, 20, 'Script_RewardChestItem'),
(900105, 0, 0, 'Epic Reward Chest (80)', 31031, 4, 0, 128, -1, -1, 80, 48356, 0, 'Right-click to claim your rewards!', -1, 80, 20, 'Script_RewardChestItem');