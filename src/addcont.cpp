#include "addcont.h"

#include "platform.h"
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <map>
#include <thread>
#include <utility>

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

constexpr uintmax_t kMaxParamBytes = 1024u * 1024u;
// Add-on icons are shown at 48 px.
constexpr int kIconPixels = 96;

bool IsUpperOrDigit(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// "UP0001-PPSA01234_00-ABCDEFGHIJKLMNOP": 2 letters, 4 digits, '-', 4 letters, 5 digits, '_', 2 digits, '-', 16 A-Z/0-9.
bool IsContentId(const std::string& id)
{
	if (id.size() != 36)
	{
		return false;
	}
	auto upper = [](char c) { return c >= 'A' && c <= 'Z'; };
	auto digit = [](char c) { return c >= '0' && c <= '9'; };
	for (size_t i = 0; i < 36; i++)
	{
		const char c  = id[i];
		bool       ok = false;
		if (i < 2 || (i >= 7 && i < 11))
		{
			ok = upper(c);
		} else if (i < 6 || (i >= 11 && i < 16) || i == 17 || i == 18)
		{
			ok = digit(c);
		} else if (i == 6 || i == 19)
		{
			ok = c == '-';
		} else if (i == 16)
		{
			ok = c == '_';
		} else
		{
			ok = IsUpperOrDigit(c);
		}
		if (!ok)
		{
			return false;
		}
	}
	return true;
}

bool IsLabel(const std::string& label)
{
	return label.size() == 16 && std::all_of(label.begin(), label.end(), IsUpperOrDigit);
}

std::string StringField(const JsonValue& obj, const char* key)
{
	auto it = obj.o.find(key);
	return it != obj.o.end() && it->second.type == JsonValue::Type::String ? it->second.s : std::string();
}

std::string AddonTitle(const JsonValue& param)
{
	auto loc = param.o.find("localizedParameters");
	if (loc != param.o.end() && loc->second.type == JsonValue::Type::Object)
	{
		const auto& l    = loc->second;
		auto        pref = l.o.find(StringField(l, "defaultLanguage"));
		if (pref != l.o.end() && pref->second.type == JsonValue::Type::Object)
		{
			if (auto t = StringField(pref->second, "titleName"); !t.empty())
			{
				return t;
			}
		}
		for (const auto& [lang, v]: l.o)
		{
			if (v.type == JsonValue::Type::Object)
			{
				if (auto t = StringField(v, "titleName"); !t.empty())
				{
					return t;
				}
			}
		}
	}
	return StringField(param, "titleName");
}

std::string ReadSmallFile(const fs::path& path)
{
	std::error_code ec;
	if (fs::file_size(path, ec) > kMaxParamBytes || ec)
	{
		return {};
	}
	return read_text_file(path_utf8(path));
}

} // namespace

std::vector<AddContentEntry> find_add_content(const std::string& addons_dir, const std::string& title_id)
{
	if (addons_dir.empty() || title_id.empty())
	{
		return {};
	}
	std::error_code ec;
	const fs::path  root = utf8_path(addons_dir);
	if (!fs::is_directory(root, ec))
	{
		return {};
	}
	std::map<std::string, AddContentEntry> by_label;
	// Folders are walked one at a time so an unreadable one does not hide its siblings. A folder holding
	// sce_sys/param.json is a package: its own data is not searched for more packages.
	std::vector<fs::path> pending {root};
	size_t                visited = 0;
	while (!pending.empty() && visited++ < 100000)
	{
		const fs::path dir = std::move(pending.back());
		pending.pop_back();
		const fs::path param_path = dir / "sce_sys" / "param.json";
		if (fs::is_regular_file(param_path, ec))
		{
			JsonValue param;
			// applicationCategoryType is set for games, not for add-ons.
			if (parse_json(ReadSmallFile(param_path), param) && param.type == JsonValue::Type::Object &&
			    param.o.count("applicationCategoryType") == 0)
			{
				const std::string content_id = StringField(param, "contentId");
				if (IsContentId(content_id) && content_id.compare(7, 9, title_id) == 0)
				{
					AddContentEntry e;
					e.content_id        = content_id;
					e.entitlement_label = content_id.substr(20);
					e.mount_dir         = path_utf8(dir);
					e.title             = AddonTitle(param);
					if (fs::is_regular_file(dir / "sce_sys" / "icon0.png", ec))
					{
						e.icon = path_utf8(dir / "sce_sys" / "icon0.png");
					}
					by_label.try_emplace(e.entitlement_label, std::move(e));
				}
			}
			continue;
		}
		for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
		{
			std::error_code ie;
			if (it->is_directory(ie) && !it->is_symlink(ie))
			{
				pending.push_back(it->path());
			}
		}
		ec.clear();
	}
	std::vector<AddContentEntry> out;
	for (auto& [label, e]: by_label)
	{
		out.push_back(std::move(e));
	}
	return out;
}

std::optional<AddContentEntry> parse_add_content(const std::string& value)
{
	const size_t      eq    = value.find('=');
	const std::string label = value.substr(0, eq);
	if (!IsLabel(label))
	{
		return std::nullopt;
	}
	AddContentEntry e;
	e.entitlement_label = label;
	if (eq != std::string::npos)
	{
		e.mount_dir = value.substr(eq + 1);
		std::error_code ec;
		if (!fs::is_directory(utf8_path(e.mount_dir), ec))
		{
			return std::nullopt;
		}
	}
	return e;
}

struct AddContentPage::Impl
{
	slint::ComponentWeakHandle<AppWindow> weak;
	SaveFn                                save;
	PickFn                                pick;

	std::string              title_id;
	std::string              game_icon;
	std::string              addons_dir;
	std::vector<std::string> entries;   // the user's own, as saved
	int                      generation = 0;

	std::shared_ptr<slint::VectorModel<AddonRow>> rows = std::make_shared<slint::VectorModel<AddonRow>>();

	const Addons* Global() const
	{
		auto h = weak.lock();
		return h ? &(*h)->global<Addons>() : nullptr;
	}

	static slint::Image Icon(const std::string& path)
	{
		std::vector<uint8_t> rgba;
		int                  w = 0;
		int                  h = 0;
		if (path.empty() || !load_image_rgba(path, kIconPixels, kIconPixels, rgba, w, h) || w <= 0 || h <= 0)
		{
			return {};
		}
		slint::SharedPixelBuffer<slint::Rgba8Pixel> buf(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
		std::memcpy(buf.begin(), rgba.data(), static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
		return slint::Image(std::move(buf));
	}

	// Lists the installed add-ons (on a thread: the add-ons folder can be big) and the user's entries.
	void Scan(const std::shared_ptr<Impl>& self)
	{
		const int gen = ++generation;
		if (const auto* a = Global())
		{
			a->set_scanning(true);
			a->set_selected(-1);
		}
		std::error_code ec;
		const bool      folder_exists = !addons_dir.empty() && fs::is_directory(utf8_path(addons_dir), ec);
		std::thread(
		    [self, gen, folder_exists, dir = addons_dir, tid = title_id, entries = entries, game_icon = game_icon]
		    {
			    auto installed = find_add_content(dir, tid);
			    slint::invoke_from_event_loop(
			        [self, gen, folder_exists, dir, entries, game_icon, installed = std::move(installed)]
			        {
				        const auto* a = self->Global();
				        if (a == nullptr || gen != self->generation)
				        {
					        return;
				        }
				        const slint::Image       fallback = Icon(game_icon);
				        std::vector<AddonRow>    rows;
				        for (const auto& e: installed)
				        {
					        AddonRow r;
					        r.title     = slint::SharedString(e.title.empty() ? e.entitlement_label : e.title);
					        r.detail    = slint::SharedString(e.content_id);
					        r.kind      = slint::SharedString("INSTALLED");
					        r.removable = false;
					        r.missing   = false;
					        const slint::Image icon = Icon(e.icon);
					        r.icon      = icon.size().width > 0 ? icon : fallback;
					        rows.push_back(std::move(r));
				        }
				        for (const auto& entry: entries)
				        {
					        const size_t      eq    = entry.find('=');
					        const std::string label = entry.substr(0, eq);
					        const std::string data  = eq == std::string::npos ? std::string() : entry.substr(eq + 1);
					        std::error_code   dec;
					        AddonRow          r;
					        r.title     = slint::SharedString(label);
					        r.removable = true;
					        r.icon      = fallback;
					        if (data.empty())
					        {
						        r.kind   = slint::SharedString("LICENSE");
						        r.detail = slint::SharedString("License only");
					        } else
					        {
						        r.kind    = slint::SharedString("DATA");
						        r.missing = !fs::is_directory(utf8_path(data), dec);
						        r.detail  = slint::SharedString((r.missing ? "Data folder not found: " : "Data: ") + data);
					        }
					        rows.push_back(std::move(r));
				        }
				        self->rows->set_vector(rows);
				        a->set_scanning(false);
				        a->set_folder_exists(folder_exists);
				        std::string status;
				        if (!rows.empty())
				        {
					        status = std::to_string(rows.size()) + (rows.size() == 1 ? " add-on" : " add-ons");
				        } else if (dir.empty())
				        {
					        status = "No add-ons folder is set. Set it in Settings > Paths to the folder that holds your extracted "
					                 "add-on (DLC) packages.";
				        } else if (!folder_exists)
				        {
					        status = "The add-ons folder does not exist: " + dir + ". Check the path in Settings > Paths.";
				        } else
				        {
					        status = "No add-ons found for this game in " + dir + ".";
				        }
				        a->set_status(slint::SharedString(status));
			        });
		    })
		    .detach();
	}
};

AddContentPage::AddContentPage(const slint::ComponentHandle<AppWindow>& ui, SaveFn save, PickFn pick)
    : m_impl(std::make_shared<Impl>())
{
	m_impl->weak = ui;
	m_impl->save = std::move(save);
	m_impl->pick = std::move(pick);
	auto& a      = ui->global<Addons>();
	a.set_rows(m_impl->rows);
	std::weak_ptr<Impl> weak_self = m_impl;
	a.on_refresh(
	    [weak_self]
	    {
		    if (auto self = weak_self.lock())
		    {
			    self->Scan(self);
		    }
	    });
	a.on_open_folder(
	    [weak_self]
	    {
		    if (auto self = weak_self.lock(); self && !self->addons_dir.empty())
		    {
			    open_in_system(self->addons_dir);
		    }
	    });
	a.on_browse_dir(
	    [weak_self]
	    {
		    auto self = weak_self.lock();
		    if (!self || !self->pick)
		    {
			    return;
		    }
		    if (const auto* g = self->Global())
		    {
			    const std::string dir = self->pick(std::string(std::string_view(g->get_dir_text())));
			    if (!dir.empty())
			    {
				    g->set_dir_text(slint::SharedString(dir));
			    }
		    }
	    });
	a.on_add(
	    [weak_self]
	    {
		    auto self = weak_self.lock();
		    if (!self)
		    {
			    return;
		    }
		    const auto* g = self->Global();
		    if (g == nullptr)
		    {
			    return;
		    }
		    // A content id ends with its entitlement label, so either can be typed.
		    std::string label(std::string_view(g->get_label_text()));
		    label.erase(0, label.find_first_not_of(" \t"));
		    label.erase(label.find_last_not_of(" \t") + 1);
		    if (const size_t dash = label.rfind('-'); dash != std::string::npos)
		    {
			    label = label.substr(dash + 1);
		    }
		    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		    std::string dir(std::string_view(g->get_dir_text()));
		    dir.erase(0, dir.find_first_not_of(" \t"));
		    dir.erase(dir.find_last_not_of(" \t") + 1);
		    if (!dir.empty())
		    {
			    std::error_code ec;
			    const fs::path  abs = fs::absolute(utf8_path(dir), ec);
			    dir                 = ec ? dir : path_utf8(abs.lexically_normal());
		    }
		    const std::string entry = dir.empty() ? label : label + "=" + dir;
		    if (!parse_add_content(entry))
		    {
			    g->set_status(slint::SharedString("Enter a 16-character entitlement label (A-Z, 0-9) or a content id, and an existing "
			                                      "data folder, or no folder for a license only."));
			    return;
		    }
		    std::erase_if(self->entries, [&](const std::string& e) { return e.substr(0, e.find('=')) == label; });
		    self->entries.push_back(entry);
		    self->save(self->title_id, self->entries);
		    g->set_label_text("");
		    g->set_dir_text("");
		    self->Scan(self);
	    });
	a.on_remove(
	    [weak_self](int row)
	    {
		    auto self = weak_self.lock();
		    if (!self || row < 0 || static_cast<size_t>(row) >= self->rows->row_count())
		    {
			    return;
		    }
		    const auto data = self->rows->row_data(static_cast<size_t>(row));
		    if (!data || !data->removable)
		    {
			    return;
		    }
		    const std::string label(std::string_view(data->title));
		    std::erase_if(self->entries, [&](const std::string& e) { return e.substr(0, e.find('=')) == label; });
		    self->save(self->title_id, self->entries);
		    self->Scan(self);
	    });
}

void AddContentPage::Open(const GameEntry& game, const std::string& addons_dir, const std::vector<std::string>& entries)
{
	auto& self      = *m_impl;
	self.title_id   = game.title_id;
	std::transform(self.title_id.begin(), self.title_id.end(), self.title_id.begin(),
	               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	self.game_icon  = game.icon;
	self.addons_dir = addons_dir;
	self.entries    = entries;
	if (const auto* a = self.Global())
	{
		a->set_game_name(slint::SharedString(game.name));
		a->set_serial(slint::SharedString(self.title_id));
		a->set_folder(slint::SharedString(addons_dir));
		a->set_label_text("");
		a->set_dir_text("");
		a->set_status("");
	}
	self.rows->set_vector({});
	self.Scan(m_impl);
}

} // namespace kyty_launcher
