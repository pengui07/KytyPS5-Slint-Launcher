#include "cheats_page.h"

#include "cheats.h"
#include "platform.h"
#include "settings.h"

#include <algorithm>
#include <filesystem>
#include <thread>
#include <utility>
#include <vector>

namespace kyty_launcher {

namespace fs = std::filesystem;

struct CheatsPage::Impl
{
	slint::ComponentWeakHandle<AppWindow> weak;

	std::string title_id;
	std::string version;
	std::string process;
	std::string run_dir;
	fs::path    path;

	CheatDoc          doc;
	bool              exists = false;
	std::vector<bool> ticks;

	std::vector<RemoteCheat> remote;
	CheatDoc                 preview;
	// Bumped by each Open (catalog) and each file pick (preview): a download that finishes later is dropped.
	int catalog_gen = 0;
	int preview_gen = 0;

	std::shared_ptr<slint::VectorModel<CheatRow>>       mods_model   = std::make_shared<slint::VectorModel<CheatRow>>();
	std::shared_ptr<slint::VectorModel<RemoteCheatRow>> remote_model = std::make_shared<slint::VectorModel<RemoteCheatRow>>();
	std::shared_ptr<slint::VectorModel<slint::SharedString>> preview_model =
	    std::make_shared<slint::VectorModel<slint::SharedString>>();

	const Cheats* Global() const
	{
		auto h = weak.lock();
		return h ? &(*h)->global<Cheats>() : nullptr;
	}

	bool Dirty() const
	{
		for (size_t i = 0; i < ticks.size() && i < doc.mods.size(); i++)
		{
			if (ticks[i] != doc.mods[i].enabled)
			{
				return true;
			}
		}
		return false;
	}

	void PushMods()
	{
		std::vector<CheatRow> rows;
		for (size_t i = 0; i < doc.mods.size(); i++)
		{
			CheatRow r;
			r.name    = slint::SharedString(doc.mods[i].name);
			r.author  = slint::SharedString(doc.mods[i].author);
			r.enabled = ticks[i];
			rows.push_back(std::move(r));
		}
		mods_model->set_vector(rows);
	}

	void SetLocalStatus(const std::string& text)
	{
		if (const auto* c = Global())
		{
			c->set_dirty(Dirty());
			c->set_local_status(slint::SharedString(text));
		}
	}

	void LoadLocal()
	{
		const auto* c = Global();
		doc           = CheatDoc();
		exists        = false;
		ticks.clear();
		std::string status;
		if (!cheats_supported(title_id))
		{
			status = "Cheats work with PS5 games (PPSA titles) only.";
		} else if (path.empty())
		{
			status = "Set the run folder in Settings > General first: the cheat file is kept there.";
		} else
		{
			const std::string error = load_cheat_file(path, doc, exists);
			if (!error.empty())
			{
				status = error + "\n" + path_utf8(path);
				doc.mods.clear();
			} else if (!exists)
			{
				status = "No cheat file yet. It will be " + path_utf8(path);
			} else
			{
				status = std::to_string(doc.mods.size()) + (doc.mods.size() == 1 ? " cheat" : " cheats") + " in " + path_utf8(path);
			}
		}
		for (const auto& m: doc.mods)
		{
			ticks.push_back(m.enabled);
		}
		PushMods();
		if (c != nullptr)
		{
			c->set_has_file(exists);
			c->set_file_path(slint::SharedString(path_utf8(path)));
		}
		SetLocalStatus(status);
	}

	void PushRemote()
	{
		std::vector<RemoteCheatRow> rows;
		for (const auto& f: remote)
		{
			RemoteCheatRow r;
			r.version   = slint::SharedString(f.version);
			r.format    = slint::SharedString(f.format == "json" ? "JSON" : f.format == "mc4" ? "MC4" : "SHN");
			r.title     = slint::SharedString(f.title.empty() ? f.name : f.title);
			r.installed = !version.empty() && f.version == version;
			r.supported = f.supported();
			rows.push_back(std::move(r));
		}
		remote_model->set_vector(rows);
	}

	void ClearPreview()
	{
		preview = CheatDoc();
		preview_model->set_vector({});
		if (const auto* c = Global())
		{
			c->set_preview_info(slint::SharedString(""));
			c->set_preview_status(slint::SharedString(""));
			c->set_preview_loading(false);
			c->set_can_import(false);
		}
	}
};

namespace {

// Runs `work` on its own thread and hands its result to `done` on the UI thread.
template <typename Work, typename Done>
void RunAsync(Work work, Done done)
{
	std::thread(
	    [work = std::move(work), done = std::move(done)]() mutable
	    {
		    auto result = work();
		    slint::invoke_from_event_loop([done = std::move(done), result = std::move(result)]() mutable { done(std::move(result)); });
	    })
	    .detach();
}

void LoadCatalog(const std::shared_ptr<CheatsPage::Impl>& self)
{
	const auto* c = self->Global();
	if (c == nullptr)
	{
		return;
	}
	const int gen = ++self->catalog_gen;
	self->preview_gen++;
	self->remote.clear();
	self->PushRemote();
	self->ClearPreview();
	c->set_remote_selected(-1);
	if (!cheats_supported(self->title_id))
	{
		c->set_remote_loading(false);
		c->set_remote_status(slint::SharedString(""));
		return;
	}
	c->set_remote_loading(true);
	c->set_remote_status("Loading the cheat collection…");
	const std::string title_id = self->title_id;
	RunAsync(
	    [title_id]
	    {
		    std::string error;
		    auto        files = fetch_remote_cheats(title_id, error);
		    return std::make_pair(std::move(files), std::move(error));
	    },
	    [self, gen](std::pair<std::vector<RemoteCheat>, std::string> result)
	    {
		    const auto* c = self->Global();
		    if (c == nullptr || gen != self->catalog_gen)
		    {
			    return;
		    }
		    // The files for the installed version first, the rest newest first.
		    auto& files = result.first;
		    std::stable_sort(files.begin(), files.end(),
		                     [&](const RemoteCheat& a, const RemoteCheat& b) { return (a.version == self->version) > (b.version == self->version); });
		    self->remote = std::move(files);
		    self->PushRemote();
		    std::string status = self->remote.empty() && result.second.empty()
		                             ? "The collection has no cheats for " + self->title_id + "."
		                             : std::to_string(self->remote.size()) + (self->remote.size() == 1 ? " file" : " files") + " for " +
		                                   self->title_id + ". Cheats by TeeKay87 / HEN Cheats Collection and its contributors.";
		    if (!result.second.empty())
		    {
			    status += "\nCould not load the whole collection (" + result.second + "). Refresh to try again.";
		    }
		    c->set_remote_loading(false);
		    c->set_remote_status(slint::SharedString(status));
	    });
}

void LoadPreview(const std::shared_ptr<CheatsPage::Impl>& self, int index)
{
	self->ClearPreview();
	const auto* c   = self->Global();
	const int   gen = ++self->preview_gen;
	if (c == nullptr || index < 0 || static_cast<size_t>(index) >= self->remote.size() || !self->remote[static_cast<size_t>(index)].supported())
	{
		return;
	}
	c->set_preview_loading(true);
	const RemoteCheat file = self->remote[static_cast<size_t>(index)];
	RunAsync(
	    [file]
	    {
		    std::string text;
		    std::string error = fetch_remote_cheat(file, text);
		    return std::make_pair(std::move(text), std::move(error));
	    },
	    [self, gen](std::pair<std::string, std::string> result)
	    {
		    const auto* c = self->Global();
		    if (c == nullptr || gen != self->preview_gen)
		    {
			    return;
		    }
		    c->set_preview_loading(false);
		    std::string error = result.second;
		    CheatDoc    doc;
		    if (error.empty())
		    {
			    error = parse_cheat_doc(result.first, doc);
		    }
		    if (!error.empty())
		    {
			    c->set_preview_status(slint::SharedString("Could not read this file: " + error));
			    return;
		    }
		    std::vector<slint::SharedString> names;
		    for (const auto& m: doc.mods)
		    {
			    names.emplace_back(m.name);
		    }
		    self->preview_model->set_vector(names);
		    std::string name = doc.root.o.count("name") != 0 && doc.root.o.at("name").type == JsonValue::Type::String ? doc.root.o.at("name").s : "";
		    std::string info = name;
		    if (const std::string credits = cheat_credits(doc); !credits.empty())
		    {
			    info += (info.empty() ? "" : "  ·  ") + std::string("Credits: ") + credits;
		    }
		    c->set_preview_info(slint::SharedString(info));
		    const std::string invalid = validate_cheat_import(doc, self->title_id, self->version, self->process);
		    c->set_preview_status(slint::SharedString(
		        invalid.empty() ? (self->exists ? "Import replaces the installed cheat file. The imported cheats start unticked."
		                                        : "Import saves these cheats for this game. They start unticked.")
		                        : invalid));
		    c->set_can_import(invalid.empty() && !self->path.empty());
		    self->preview = std::move(doc);
	    });
}

} // namespace

CheatsPage::CheatsPage(const slint::ComponentHandle<AppWindow>& ui): m_impl(std::make_shared<Impl>())
{
	m_impl->weak = slint::ComponentWeakHandle<AppWindow>(ui);
	const auto& c = ui->global<Cheats>();
	c.set_mods(m_impl->mods_model);
	c.set_remote(m_impl->remote_model);
	c.set_preview(m_impl->preview_model);
	std::weak_ptr<Impl> weak = m_impl;

	c.on_toggle(
	    [weak](int i, bool on)
	    {
		    auto self = weak.lock();
		    if (!self || i < 0 || static_cast<size_t>(i) >= self->ticks.size())
		    {
			    return;
		    }
		    self->ticks[static_cast<size_t>(i)] = on;
		    auto row                            = self->mods_model->row_data(static_cast<size_t>(i));
		    if (row.has_value())
		    {
			    row->enabled = on;
			    self->mods_model->set_row_data(static_cast<size_t>(i), *row);
		    }
		    self->SetLocalStatus(self->Dirty() ? "Unsaved changes: Save to use them the next time the game starts." : "No changes.");
	    });
	c.on_set_all(
	    [weak](bool on)
	    {
		    auto self = weak.lock();
		    if (!self)
		    {
			    return;
		    }
		    std::fill(self->ticks.begin(), self->ticks.end(), on);
		    self->PushMods();
		    self->SetLocalStatus(self->Dirty() ? "Unsaved changes: Save to use them the next time the game starts." : "No changes.");
	    });
	c.on_save(
	    [weak]
	    {
		    auto self = weak.lock();
		    if (!self)
		    {
			    return;
		    }
		    const std::string error = save_cheat_file(self->path, self->doc, self->ticks, self->exists ? self->doc.text : std::string());
		    self->SetLocalStatus(error.empty() ? "Saved. The ticked cheats apply the next time the game starts." : error);
	    });
	c.on_reload(
	    [weak]
	    {
		    if (auto self = weak.lock())
		    {
			    self->LoadLocal();
		    }
	    });
	c.on_open_folder(
	    [weak]
	    {
		    auto self = weak.lock();
		    if (!self || self->path.empty())
		    {
			    return;
		    }
		    std::error_code ec;
		    const fs::path  dir = self->path.parent_path();
		    open_in_system(path_utf8(fs::is_directory(dir, ec) ? dir : utf8_path(self->run_dir)));
	    });
	c.on_refresh(
	    [weak]
	    {
		    if (auto self = weak.lock())
		    {
			    LoadCatalog(self);
		    }
	    });
	c.on_select_remote(
	    [weak](int i)
	    {
		    if (auto self = weak.lock())
		    {
			    LoadPreview(self, i);
		    }
	    });
	c.on_import(
	    [weak]
	    {
		    auto self = weak.lock();
		    if (!self)
		    {
			    return;
		    }
		    const auto* c = self->Global();
		    std::string error = validate_cheat_import(self->preview, self->title_id, self->version, self->process);
		    if (error.empty())
		    {
			    CheatDoc imported = self->preview;
			    error = save_cheat_file(self->path, imported, std::vector<bool>(imported.mods.size(), false), self->exists ? self->doc.text : std::string());
		    }
		    if (!error.empty())
		    {
			    if (c != nullptr)
			    {
				    c->set_preview_status(slint::SharedString(error));
			    }
			    return;
		    }
		    const size_t count = self->preview.mods.size();
		    self->LoadLocal();
		    self->SetLocalStatus("Imported " + std::to_string(count) + (count == 1 ? " cheat" : " cheats") +
		                         ". Tick the ones you want and Save.");
		    if (c != nullptr)
		    {
			    c->set_preview_status(slint::SharedString("Imported."));
		    }
	    });
}

void CheatsPage::Open(const GameEntry& game, const std::string& run_dir)
{
	auto& self    = *m_impl;
	self.title_id = game.title_id;
	self.version  = game.game_version;
	self.process  = path_utf8(utf8_path(game.eboot).filename());
	if (self.process.empty())
	{
		self.process = "eboot.bin";
	}
	self.run_dir = run_dir;
	self.path    = cheat_file_path(run_dir, game.title_id);
	if (const auto* c = self.Global())
	{
		c->set_game_name(slint::SharedString(game.name));
		c->set_serial(slint::SharedString(game.title_id));
		c->set_game_version(slint::SharedString(game.game_version));
	}
	self.LoadLocal();
	LoadCatalog(m_impl);
}

} // namespace kyty_launcher
