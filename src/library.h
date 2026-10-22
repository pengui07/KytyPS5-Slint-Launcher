#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace kyty_launcher {

// Same states as the Qt launcher's compatibility column.
enum class GameStatus { Unknown, InGame, MainMenu, Logo, DoesntBoot };

struct GameEntry
{
	std::string name;
	std::string title_id;
	std::string game_version;
	std::string firmware_version; // empty when param.json has none
	std::string dir;
	std::string eboot;
	std::string icon; // sce_sys/icon0.png, empty when missing
	std::string background; // sce_sys/pic1.png (or pic0.png), empty when missing
	uint64_t    size   = 0;
	GameStatus  status = GameStatus::Unknown;
	std::string comment;
	std::string play_time; // e.g. "12h 30m"; empty until play time is tracked (the Play Time column)
};

struct CompatEntry
{
	GameStatus  status = GameStatus::Unknown;
	std::string comment;
};

using CompatDb  = std::map<std::string, CompatEntry>; // key: upper-case title id
using SizeCache = std::map<std::string, uint64_t>;    // key: path_key(dir)

// Finds game folders (a folder holding eboot.bin) under the ';'-separated roots and reads
// their sce_sys/param.json. Sorted by name. Folder sizes already in `sizes` are reused.
std::vector<GameEntry> scan_games(const std::string& roots, SizeCache& sizes);

CompatDb parse_compat(const std::string& json);
void     apply_compat(std::vector<GameEntry>& games, const CompatDb& db);

// Downloads the shared compatibility list; empty on failure.
std::string fetch_compat_json();

// Decodes an image file and scales it down to fit max_w x max_h (never up). Output is
// non-premultiplied RGBA8. Windows only (WIC); returns false elsewhere or on failure.
bool load_image_rgba(const std::string& path, int max_w, int max_h, std::vector<uint8_t>& rgba, int& w, int& h);

std::string read_text_file(const std::string& path);
bool        write_text_file(const std::string& path, const std::string& text); // creates the folder

// The titleId in sce_sys/param.json of a game ("" when there is none). `game` is the eboot.bin path, or the
// game's folder. Only [A-Za-z0-9_-] titles are returned (it names files).
std::string read_title_id(const std::string& game);

const char* status_text(GameStatus s);
std::string format_size(uint64_t bytes);

// Lower-cased, forward-slash form for comparing paths typed in different styles.
std::string path_key(const std::string& path);

// STEP6:6c: the library's sort keys, in header order.
enum class GameSortKey { Name, Status, Serial, Firmware, Size, Version, Playtime, Path, Comment };

// "size" -> GameSortKey::Size; anything unknown -> Name (the default order).
GameSortKey game_sort_key_from_string(const std::string& key);
// The saved form of a key ("size"), never null.
const char* game_sort_key_name(GameSortKey key);

// The key a game is pinned ("Pin to top") by: its title id, or the eboot path for a game without one.
// Normalised with path_key, so it compares the same however the user typed it.
std::string favourite_key(const GameEntry& game);

// Orders the library: games whose favourite_key is in `favourites` first, then `key`. Descending only flips
// `key`: the pinned group stays on top and ties always break by name, then title id, so the order is stable.
void sort_games(std::vector<GameEntry>& games, GameSortKey key, bool descending, const std::set<std::string>& favourites);

} // namespace kyty_launcher
