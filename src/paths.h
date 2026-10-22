#pragma once

#include <filesystem>
#include <string>

namespace kyty_launcher {

// Where the launcher keeps its files, computed once in main(). Nothing is created here; writers create
// their folders on demand.
//
// Portable mode (a file named portable.txt next to the launcher exe; on macOS next to the .app bundle):
//   everything under <exe dir>\ (the bundle's parent on macOS).
// Otherwise a per-user root: Windows %LOCALAPPDATA%\Kyty, macOS ~/Library/Application Support/Kyty,
//   Linux $XDG_CONFIG_HOME/kyty (configs) and $XDG_DATA_HOME/kyty (the rest).
//
//   <config_dir>\kyty.json              user config
//   <config_dir>\games\<TITLEID>.json   per-game overrides
//   <root>\versions\  <root>\cache\  <root>\logs\  <root>\run\
// where config_dir is <root>\configs, except on Linux (non-portable) where it is $XDG_CONFIG_HOME/kyty itself.
struct Paths
{
	bool                  portable = false;
	std::filesystem::path exe_dir;          // the launcher's own folder (the emulator may sit next to it)
	std::filesystem::path root;             // data root (Linux, not portable: the XDG data dir)
	std::filesystem::path config_dir;       // kyty.json lives here
	std::filesystem::path games_config_dir; // per-game override files
	std::filesystem::path versions_dir;     // default folder for downloaded emulator builds
	std::filesystem::path cache_dir;        // release lists, compatibility list, trophy icons
	std::filesystem::path logs_dir;
	std::filesystem::path run_dir;          // default emulator working directory

	std::filesystem::path settings_file() const { return config_dir / "kyty.json"; }
	std::filesystem::path game_config_file(const std::string& title_id) const { return games_config_dir / (title_id + ".json"); }
	std::filesystem::path compat_cache_file() const { return cache_dir / "compat.json"; }
	std::filesystem::path trophy_cache_dir(const std::string& title_id) const
	{
		return cache_dir / "trophies" / (title_id.empty() ? std::string("unknown") : title_id);
	}
};

// The folder holding the launcher executable.
std::filesystem::path exe_dir(const char* argv0);

// The Linux and macOS branches are untested.
Paths compute_paths(const char* argv0);

} // namespace kyty_launcher
