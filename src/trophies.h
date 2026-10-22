#pragma once

#include <array>
#include <string>
#include <vector>

namespace kyty_launcher {

// Trophy counts of one game. Grade index: 1 platinum, 2 gold, 3 silver, 4 bronze.
struct TrophyProgress
{
	std::array<int, 5> total_grade {};
	std::array<int, 5> earned_grade {};
	int                total  = 0;
	int                earned = 0;

	// Same as the emulator's: points (gold 6, silver 2, bronze 1; platinum not counted), in percent.
	int Percent() const;
};

// Progress of the game in `game_dir`: the trophies come from its sce_sys/trophy2/trophyNN.ucp
// packages, the unlocked ones from <run_dir>\_Trophies\<title_id>\trophies_<user_id>_<NN>.json
// (written by the emulator). False when the game has no trophy package.
bool load_trophy_progress(const std::string& game_dir, const std::string& title_id, const std::string& run_dir, int user_id,
                          TrophyProgress& out);

// One trophy of a game, for its trophy page.
struct TrophyInfo
{
	int         grade = 0; // 1 platinum .. 4 bronze
	std::string name;
	std::string detail;
	std::string icon;      // PNG copied out of the package into icon_cache_dir; empty when it has none
	bool        hidden = false;
	bool        earned = false;
	std::string earned_at; // "2026-10-19 14:05" (UTC), empty when unknown
};

// Every trophy of the game, texts in `console_language` (the launcher's 0..29 index), icons
// written under `icon_cache_dir`. Sorted by package, then id.
bool load_trophy_list(const std::string& game_dir, const std::string& title_id, const std::string& run_dir, int user_id,
                      int console_language, const std::string& icon_cache_dir, std::vector<TrophyInfo>& out);

} // namespace kyty_launcher
