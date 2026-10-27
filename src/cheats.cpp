#include "cheats.h"

#include "net.h"
#include "text.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

constexpr size_t kIndexLimit = 2 * 1024 * 1024;
constexpr size_t kFileLimit  = 8 * 1024 * 1024;

const char* const kBaseUrl = "https://raw.githubusercontent.com/TeeKay87/HEN-Cheats-Collection/master/cheats/";

bool EqualNoCase(const std::string& a, const std::string& b)
{
	return to_upper(a) == to_upper(b);
}

const JsonValue* Member(const JsonValue& o, const char* key)
{
	auto it = o.o.find(key);
	return o.type == JsonValue::Type::Object && it != o.o.end() ? &it->second : nullptr;
}

std::string StringMember(const JsonValue& o, const char* key)
{
	const auto* v = Member(o, key);
	return v != nullptr && v->type == JsonValue::Type::String ? v->s : std::string();
}

bool IsHex(const std::string& s)
{
	return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

// The collection's file names: <TITLEID>_<version>_<anything>.<format>.
bool MatchName(const std::string& name, std::string* title, std::string* version, std::string* format)
{
	static const std::regex pattern(R"(^([A-Z]{4}[0-9]{5})_([0-9]+(?:\.[0-9]+)+)_[A-Za-z0-9_.-]+\.(json|mc4|shn)$)");
	std::smatch             m;
	if (!std::regex_match(name, m, pattern))
	{
		return false;
	}
	*title   = m[1];
	*version = m[2];
	*format  = m[3];
	return true;
}

std::string Download(const std::string& url, size_t limit, std::string& body)
{
	body.clear();
	bool        too_large = false;
	std::string error;
	const bool  ok = http_get(
        url,
        [&](const char* data, size_t n)
        {
	        if (body.size() + n > limit)
	        {
		        too_large = true;
		        return false;
	        }
	        body.append(data, n);
	        return true;
        },
        nullptr, &error);
	if (too_large)
	{
		return "the download is larger than expected";
	}
	if (!ok)
	{
		return error.empty() ? std::string("download failed") : error;
	}
	return {};
}

} // namespace

bool cheats_supported(const std::string& title_id)
{
	static const std::regex pattern("^PPSA[0-9]{5}$");
	return std::regex_match(to_upper(trim(title_id)), pattern);
}

fs::path cheat_file_path(const std::string& run_dir, const std::string& title_id)
{
	if (!cheats_supported(title_id) || run_dir.empty())
	{
		return {};
	}
	return utf8_path(run_dir) / "_Patches" / (to_upper(trim(title_id)) + ".json");
}

std::string parse_cheat_doc(const std::string& text, CheatDoc& out)
{
	out = CheatDoc();
	std::string body = text;
	if (body.rfind("\xEF\xBB\xBF", 0) == 0)
	{
		body.erase(0, 3);
	}
	JsonValue root;
	bool      parsed = false;
	try
	{
		parsed = parse_json(body, root);
	} catch (...)
	{
		parsed = false;
	}
	if (!parsed || root.type != JsonValue::Type::Object)
	{
		return "Not a valid cheat file (bad JSON).";
	}
	const auto* mods = Member(root, "mods");
	if (mods == nullptr || mods->type != JsonValue::Type::Array)
	{
		return "The cheat file has no \"mods\" list.";
	}
	for (const auto& m: mods->a)
	{
		const auto* name    = Member(m, "name");
		const auto* enabled = Member(m, "enabled");
		if (m.type != JsonValue::Type::Object || name == nullptr || name->type != JsonValue::Type::String || name->s.empty() ||
		    (enabled != nullptr && enabled->type != JsonValue::Type::Bool))
		{
			return "The cheat file has an invalid entry.";
		}
		out.mods.push_back({name->s, StringMember(m, "author"), enabled == nullptr || enabled->b});
	}
	out.root = std::move(root);
	out.text = text;
	return {};
}

std::string load_cheat_file(const fs::path& path, CheatDoc& out, bool& exists)
{
	out = CheatDoc();
	std::error_code ec;
	exists = fs::exists(path, ec);
	if (!exists)
	{
		return {};
	}
	std::ifstream f(path, std::ios::binary);
	if (!f)
	{
		return "Could not read " + path_utf8(path);
	}
	std::stringstream ss;
	ss << f.rdbuf();
	const std::string text  = ss.str();
	std::string       error = parse_cheat_doc(text, out);
	out.text                = text; // kept even when invalid: a later save must still see it unchanged
	return error;
}

std::string save_cheat_file(const fs::path& path, CheatDoc& doc, const std::vector<bool>& enabled, const std::string& expected)
{
	if (path.empty() || doc.root.type != JsonValue::Type::Object || enabled.size() != doc.mods.size())
	{
		return "Nothing to save.";
	}
	std::error_code ec;
	if (fs::exists(path, ec))
	{
		std::ifstream     f(path, std::ios::binary);
		std::stringstream ss;
		ss << f.rdbuf();
		if (!f || ss.str() != expected)
		{
			return "The cheat file changed on disk since it was loaded. Reload it, then try again.";
		}
	} else if (!expected.empty())
	{
		return "The cheat file was deleted since it was loaded. Reload it, then try again.";
	}
	JsonValue root = doc.root;
	auto&     mods = root.o["mods"];
	for (size_t i = 0; i < mods.a.size() && i < enabled.size(); i++)
	{
		JsonValue on;
		on.type                 = JsonValue::Type::Bool;
		on.b                    = enabled[i];
		mods.a[i].o["enabled"] = on;
	}
	const std::string text = dump_json(root);
	fs::create_directories(path.parent_path(), ec);
	fs::path tmp = path;
	tmp += ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		out << text;
		if (!out)
		{
			return "Could not write " + path_utf8(tmp);
		}
	}
	fs::rename(tmp, path, ec);
	if (ec)
	{
		fs::remove(tmp, ec);
		return "Could not save " + path_utf8(path);
	}
	doc.root = std::move(root);
	doc.text = text;
	for (size_t i = 0; i < doc.mods.size(); i++)
	{
		doc.mods[i].enabled = enabled[i];
	}
	return {};
}

std::string validate_cheat_import(const CheatDoc& doc, const std::string& title_id, const std::string& version, const std::string& process)
{
	const JsonValue& root = doc.root;
	if (!cheats_supported(title_id) || !EqualNoCase(StringMember(root, "id"), trim(title_id)))
	{
		return "This file is not for this game's title ID.";
	}
	if (version.empty() || StringMember(root, "version") != version)
	{
		return "This file is for version " + StringMember(root, "version") + ", not the installed " +
		       (version.empty() ? std::string("(unknown)") : version) + ".";
	}
	if (process.empty() || !EqualNoCase(StringMember(root, "process"), process))
	{
		return "This file is for another executable (" + StringMember(root, "process") + ", not " + process + ").";
	}
	const auto* mods = Member(root, "mods");
	if (mods == nullptr || mods->a.empty())
	{
		return "The file has no cheats.";
	}
	if (Member(root, "master") != nullptr)
	{
		return "This file uses master codes, which the emulator does not support yet.";
	}
	for (const auto& m: mods->a)
	{
		const std::string name = StringMember(m, "name");
		if (Member(m, "module_name") != nullptr)
		{
			return "This file has module-specific cheats, which the emulator does not support yet.";
		}
		const auto* memory = Member(m, "memory");
		if (memory == nullptr || memory->type != JsonValue::Type::Array || memory->a.empty())
		{
			return "Unsupported memory entries in cheat \"" + name + "\".";
		}
		for (const auto& w: memory->a)
		{
			const std::string offset   = StringMember(w, "offset");
			const std::string off      = StringMember(w, "off");
			const std::string on       = StringMember(w, "on");
			const auto*       absolute = Member(w, "absolute");
			const auto*       expected = Member(w, "expected");
			if ((absolute != nullptr && (absolute->type != JsonValue::Type::Bool || absolute->b)) ||
			    (expected != nullptr && (expected->type != JsonValue::Type::String || !EqualNoCase(expected->s, off))) ||
			    Member(w, "section") != nullptr || Member(w, "sectionName") != nullptr || Member(w, "sectionProtection") != nullptr ||
			    Member(w, "logicalOffset") != nullptr || Member(w, "sectionOffset") != nullptr)
			{
				return "This file uses memory addressing the emulator does not support yet.";
			}
			std::string hex = offset;
			if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
			{
				hex.erase(0, 2);
			}
			if (w.type != JsonValue::Type::Object || !IsHex(hex) || hex.size() > 16 || !IsHex(off) || !IsHex(on) ||
			    off.size() % 2 != 0 || off.size() != on.size())
			{
				return "Unsupported memory entries in cheat \"" + name + "\".";
			}
		}
	}
	return {};
}

std::string cheat_credits(const CheatDoc& doc)
{
	std::string credits;
	if (const auto* list = Member(doc.root, "credits"); list != nullptr && list->type == JsonValue::Type::Array)
	{
		for (const auto& c: list->a)
		{
			if (c.type == JsonValue::Type::String && !c.s.empty())
			{
				credits += (credits.empty() ? "" : ", ") + c.s;
			}
		}
	}
	return credits.empty() ? StringMember(doc.root, "author") : credits;
}

std::string RemoteCheat::url() const
{
	return std::string(kBaseUrl) + format + "/" + name;
}

std::vector<RemoteCheat> fetch_remote_cheats(const std::string& title_id, std::string& error)
{
	error.clear();
	const std::string        id = to_upper(trim(title_id));
	std::vector<RemoteCheat> files;
	std::set<std::string>    seen;
	for (const char* format: {"json", "mc4", "shn"})
	{
		std::string body;
		std::string e = Download(std::string(kBaseUrl) + format + ".txt", kIndexLimit, body);
		if (!e.empty())
		{
			error += (error.empty() ? "" : "\n") + to_upper(format) + " index: " + e;
			continue;
		}
		std::istringstream lines(body);
		for (std::string line; std::getline(lines, line);)
		{
			line = trim(line);
			if (line.rfind("\xEF\xBB\xBF", 0) == 0)
			{
				line.erase(0, 3);
			}
			const size_t eq = line.find('=');
			if (eq == std::string::npos)
			{
				continue;
			}
			RemoteCheat f;
			f.name = line.substr(0, eq);
			std::string title;
			if (!MatchName(f.name, &title, &f.version, &f.format) || f.format != format || title != id || !seen.insert(f.name).second)
			{
				continue;
			}
			f.title = trim(line.substr(eq + 1));
			files.push_back(std::move(f));
		}
	}
	std::sort(files.begin(), files.end(),
	          [](const RemoteCheat& a, const RemoteCheat& b)
	          {
		          if (a.version != b.version)
		          {
			          return a.version > b.version;
		          }
		          if (a.format != b.format)
		          {
			          return a.format < b.format;
		          }
		          return a.name < b.name;
	          });
	return files;
}

std::string fetch_remote_cheat(const RemoteCheat& file, std::string& text)
{
	if (!file.supported())
	{
		return "Only JSON cheat files are supported.";
	}
	return Download(file.url(), kFileLimit, text);
}

} // namespace kyty_launcher
