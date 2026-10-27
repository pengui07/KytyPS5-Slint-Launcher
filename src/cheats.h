#pragma once

#include "settings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace kyty_launcher {

// Cheats (the emulator's --game-patch): one GoldHEN-style JSON file per game, <run folder>\_Patches\<TITLEID>.json,
// the same file the Qt launcher's "Cheats (experimental)" dialog writes, so its cheats carry over. The file's "mods"
// each have a name, an "enabled" flag and the memory writes; the emulator applies the enabled ones at boot when the
// file's id, version and process match the game.

// Only PS5 titles (PPSA + 5 digits) take cheats, as in the Qt launcher.
bool cheats_supported(const std::string& title_id);

// <run_dir>\_Patches\<TITLEID>.json; empty when the title is not supported or there is no run folder.
std::filesystem::path cheat_file_path(const std::string& run_dir, const std::string& title_id);

struct CheatMod
{
	std::string name;
	std::string author; // the mod's "author", empty when it has none
	bool        enabled = true;
};

// A cheat file as read. `text` is the file's bytes, kept so a save can refuse to overwrite a file that changed
// since (another launcher, or a hand edit).
struct CheatDoc
{
	JsonValue             root;
	std::vector<CheatMod> mods;
	std::string           text;
};

// Parses cheat JSON: an object with a "mods" array of named entries. "" on success, else why not.
std::string parse_cheat_doc(const std::string& text, CheatDoc& out);

// Reads the local file. `exists` false (and "" returned) when there is none yet.
std::string load_cheat_file(const std::filesystem::path& path, CheatDoc& out, bool& exists);

// Writes `doc` with each mod's "enabled" set from `enabled` (same length as doc.mods). `expected` is the text the
// file held when it was read ("" = it did not exist): a file that differs now is not overwritten. On success
// `doc` is updated to what was written. "" on success, else why not.
std::string save_cheat_file(const std::filesystem::path& path, CheatDoc& doc, const std::vector<bool>& enabled,
                            const std::string& expected);

// The extra checks a downloaded file must pass before it replaces the local one: it is for this title, version and
// executable, and uses only the memory writes the emulator supports. "" when it can be imported.
std::string validate_cheat_import(const CheatDoc& doc, const std::string& title_id, const std::string& version,
                                  const std::string& process);

// "Credits: a, b" (or the author), from the file's "credits" or "author".
std::string cheat_credits(const CheatDoc& doc);

// One file of the online collection (TeeKay87/HEN-Cheats-Collection).
struct RemoteCheat
{
	std::string name;    // PPSA01234_01.000_xxx.json
	std::string title;   // the index's description
	std::string version; // the game version the file is for
	std::string format;  // json, mc4 or shn; only json can be used
	bool        supported() const { return format == "json"; }
	std::string url() const;
};

// Downloads the collection's json/mc4/shn indexes and returns this title's files, newest version first (blocking:
// call it off the UI thread). `error` gets one line per index that could not be read.
std::vector<RemoteCheat> fetch_remote_cheats(const std::string& title_id, std::string& error);

// Downloads one file of the collection (blocking). "" on success.
std::string fetch_remote_cheat(const RemoteCheat& file, std::string& text);

} // namespace kyty_launcher
