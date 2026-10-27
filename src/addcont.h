#pragma once

#include "app.h"
#include "library.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kyty_launcher {

// Additional content (add-ons, DLC) of a game, as the Qt launcher's Additional Content dialog handles it.
//  - Installed: extracted add-on packages in the shared add-ons folder (Settings::addons_dir, --addcont-dir). The
//    emulator finds them itself; the launcher only lists the ones made for the game.
//  - The user's own entries (Settings::addcont): an entitlement label for a license-only add-on ("LABEL"), or a
//    label with its data folder ("LABEL=DIR"). Each one is passed as --addcont.
struct AddContentEntry
{
	std::string content_id;        // "UP0001-PPSA01234_00-ABCDEFGHIJKLMNOP"; empty for a user entry
	std::string entitlement_label; // the last 16 characters of the content id
	std::string mount_dir;         // the add-on's data folder; empty for a license only
	std::string title;             // from the add-on's param.json
	std::string icon;              // its sce_sys/icon0.png, empty when it has none
};

// The packages for `title_id` in the add-ons folder (any folder holding sce_sys/param.json with an add-on's
// content id for that title), by entitlement label.
std::vector<AddContentEntry> find_add_content(const std::string& addons_dir, const std::string& title_id);

// "LABEL" or "LABEL=DIR": the label is 16 characters A-Z/0-9 and DIR, when given, an existing folder.
std::optional<AddContentEntry> parse_add_content(const std::string& value);

// Drives the Addons global of ui/addons.slint.
class AddContentPage
{
public:
	// `save(title_id, entries)` stores a game's user entries (an empty list removes them) and writes the settings.
	using SaveFn = std::function<void(const std::string& title_id, const std::vector<std::string>& entries)>;
	using PickFn = std::function<std::string(const std::string& current)>; // a folder, "" when cancelled

	AddContentPage(const slint::ComponentHandle<AppWindow>& ui, SaveFn save, PickFn pick);
	AddContentPage(const AddContentPage&)            = delete;
	AddContentPage& operator=(const AddContentPage&) = delete;

	// Shows `game`'s add-ons: the installed ones in `addons_dir` and the user's `entries`.
	void Open(const GameEntry& game, const std::string& addons_dir, const std::vector<std::string>& entries);

	struct Impl;

private:
	std::shared_ptr<Impl> m_impl;
};

} // namespace kyty_launcher
