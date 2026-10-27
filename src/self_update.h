#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace kyty_launcher {

// The launcher's own updates (Help > Check for updates), from the GitHub releases of this repository. A release
// carries one archive per platform, named "<anything>-Windows-x64.zip" or "<anything>-Linux-x86_64.tar.gz", holding
// the launcher executable (the release workflow, .github/workflows/release.yml, makes them). Emulator builds are
// the Version Manager's business, not this.

// Where the releases come from.
constexpr const char* kLauncherRepo = "pengui07/KytyPS5-Slint-Launcher";

struct LauncherRelease
{
	std::string tag;        // "v1.2.0"
	std::string name;       // the release title
	std::string notes;      // the release text (Markdown)
	std::string page_url;   // https://github.com/<repo>/releases/tag/<tag>
	std::string asset_name; // this platform's archive; empty when the release has none
	std::string asset_url;
	uint64_t    asset_size = 0;
};

// This build's release tag (KYTY_LAUNCHER_VERSION, set by the release workflow); "" for a development build.
std::string launcher_version();

// Whether this build can update itself: a release build on a platform the releases have an archive for.
bool launcher_updates_supported();

// < 0 when `a` is older than `b`, 0 when equal, > 0 when newer. Tags are compared by their numbers
// ("v1.10.0" > "v1.9.2"); a tag without numbers is never newer.
int compare_launcher_versions(const std::string& a, const std::string& b);

// The latest published release (drafts and pre-releases are not offered). False with `error` on failure.
bool fetch_latest_launcher_release(LauncherRelease& out, std::string* error);

// The full path of the running launcher executable ("" when it can't be told).
std::string current_executable();

// Downloads `release`'s archive, unpacks it and swaps its files in next to `exe` (the running launcher). Each file
// that is replaced is renamed to "<name>.old" first (Windows allows that for a running executable) and the next
// start deletes it. Nothing is changed when a step fails or `cancel` is set. `new_exe` gets the launcher to restart.
bool install_launcher_update(const LauncherRelease& release, const std::string& exe,
                             const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel,
                             std::string* new_exe, std::string* error);

// Deletes what an earlier update left in `exe_dir`: the replaced "*.old" files and a stale work folder.
void cleanup_launcher_update(const std::string& exe_dir);

// Starts `exe` as a new process (no console), detached from this one. False when it could not be started.
bool start_detached(const std::string& exe);

} // namespace kyty_launcher
