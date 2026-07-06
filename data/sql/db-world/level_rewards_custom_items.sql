DELETE FROM `item_template` WHERE `entry` BETWEEN 900100 AND 900105;

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `AllowableClass`, `AllowableRace`, `RequiredLevel`, `SpellID_1`, `SpellTrigger_1`, `Description`, `Material`, `ItemLevel`, `stackable`, `ScriptName`) VALUES
(900100, 0, 0, 'Common Reward Chest', 31031, 1, 4, 128, -1, -1, 1, 0, 0, 'Right-click to claim your rewards!', -1, 1, 20, 'Script_RewardChestItem'),
(900101, 0, 0, 'Uncommon Reward Chest', 31031, 2, 4, 128, -1, -1, 20, 0, 0, 'Right-click to claim your rewards!', -1, 20, 20, 'Script_RewardChestItem'),
(900102, 0, 0, 'Rare Reward Chest', 31031, 3, 4, 128, -1, -1, 40, 0, 0, 'Right-click to claim your rewards!', -1, 40, 20, 'Script_RewardChestItem'),
(900103, 0, 0, 'Epic Reward Chest (60)', 31031, 4, 4, 128, -1, -1, 60, 0, 0, 'Right-click to claim your rewards!', -1, 60, 20, 'Script_RewardChestItem'),
(900104, 0, 0, 'Epic Reward Chest (70)', 31031, 4, 4, 128, -1, -1, 70, 0, 0, 'Right-click to claim your rewards!', -1, 70, 20, 'Script_RewardChestItem'),
(900105, 0, 0, 'Epic Reward Chest (80)', 31031, 4, 4, 128, -1, -1, 80, 0, 0, 'Right-click to claim your rewards!', -1, 80, 20, 'Script_RewardChestItem');