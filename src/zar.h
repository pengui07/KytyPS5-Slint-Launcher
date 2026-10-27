#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace kyty_launcher {

// .zar game archives (the emulator's ZArchive format, https://github.com/Exzap/ZArchive): a game folder packed
// into one compressed file that the emulator runs as it is. The launcher reads the game's sce_sys files from
// inside one (name, serial, icon, art, trophies) and can pack a folder into one (Library > Compress to ZAR).

// Whether `path` ends in .zar (any case). The file itself is not looked at.
bool is_zar_path(const std::string& path);

// A file inside an archive is named "<archive>!/<member>", as in the Qt launcher and the emulator, so every
// place that takes a path to an image or a param.json can also take one inside a .zar.
std::string zar_member_path(const std::string& archive, const std::string& member);
// Splits a "<archive>!/<member>" path; false for an ordinary path.
bool split_zar_member_path(const std::string& path, std::string* archive, std::string* member);

class ZarReader
{
public:
	struct Entry
	{
		std::string name;
		bool        is_file = false;
	};

	// Null when the file is missing or not a valid archive.
	static std::shared_ptr<ZarReader> Open(const std::string& archive);
	~ZarReader();
	ZarReader(const ZarReader&)            = delete;
	ZarReader& operator=(const ZarReader&) = delete;

	bool FileExists(const std::string& member);
	// The whole file, when it exists and is at most `max_size` bytes.
	bool ReadFile(const std::string& member, uint64_t max_size, std::string& out);
	// The entries of a folder inside the archive ("" is the root); empty when it is not a folder.
	std::vector<Entry> List(const std::string& member);

private:
	struct Impl;
	explicit ZarReader(std::unique_ptr<Impl> impl);
	std::unique_ptr<Impl> m_impl;
};

// Reads a file from disk or, for a "<archive>!/<member>" path, from inside the archive. False when it is missing,
// unreadable or bigger than `max_size`.
bool read_game_file(const std::string& path, uint64_t max_size, std::string& out);

// Packs `directory` (everything below it, following folder links like the emulator does) into a new `archive`.
// `progress(done, total)` gets the bytes packed so far; `cancel` (optional) stops the pack. A failed or cancelled
// pack deletes the half-written archive. An archive that already exists is never overwritten.
bool pack_zar(const std::string& directory, const std::string& archive, const std::function<void(uint64_t, uint64_t)>& progress,
              const std::atomic<bool>* cancel, std::string* error);

} // namespace kyty_launcher
