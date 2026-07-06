CREATE TABLE IF NOT EXISTS `mod_level_rewards_items` (
    `level` tinyint unsigned NOT NULL DEFAULT 0,
    `money` int unsigned NOT NULL DEFAULT 0,
    `spell` int unsigned NOT NULL DEFAULT 0,
    `learn` boolean NOT NULL DEFAULT false,
    `itemId1` int unsigned NOT NULL DEFAULT 0,
    `itemId2` int unsigned NOT NULL DEFAULT 0,
    `race` tinyint unsigned NOT NULL DEFAULT 0,
    `class` tinyint unsigned NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `mod_level_rewards_items` (`level`, `money`, `spell`, `learn`, `itemId1`, `itemId2`, `race`, `class`) VALUES
(10, 1, 20217, 0, 900100, 0, 0, 0),
(20, 4, 48161, 0, 900101, 0, 0, 0),
(30, 8, 48469, 0, 900101, 0, 0, 0),
(40, 10, 20217, 0, 900102, 0, 0, 0),
(50, 15, 48161, 0, 900102, 0, 0, 0),
(60, 20, 48469, 0, 900103, 0, 0, 0),
(70, 30, 48161, 0, 900104, 0, 0, 0),
(80, 50, 20217, 0, 900105, 0, 0, 0);