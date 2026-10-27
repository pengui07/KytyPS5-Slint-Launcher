#pragma once

#include "app.h"
#include "library.h"

#include <memory>
#include <string>

namespace kyty_launcher {

// Drives the Cheats global of ui/cheats.slint: the game's cheat file (see cheats.h) and the online collection.
// Downloads run on their own threads; their results are dropped when another game (or file) was opened since.
class CheatsPage
{
public:
	explicit CheatsPage(const slint::ComponentHandle<AppWindow>& ui);
	CheatsPage(const CheatsPage&)            = delete;
	CheatsPage& operator=(const CheatsPage&) = delete;

	// Shows `game`'s cheats; the file lives under `run_dir` (the emulator's working folder).
	void Open(const GameEntry& game, const std::string& run_dir);

	struct Impl; // public so the page's download helpers (cheats_page.cpp) can take it

private:
	std::shared_ptr<Impl> m_impl;
};

} // namespace kyty_launcher
