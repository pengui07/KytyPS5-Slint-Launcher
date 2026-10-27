#include "themes.h"

#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

// ui/common.slint keeps the classic values as its defaults, so a window looks right before main.cpp applies a theme.
const uint32_t kDark[] = {
	0xffffff0f, // row-hover
	0xffffff1a, // row-selected
	0xe7ecf3cc, // row-outline
	0x11141ab8, // card-glass
	0x0a0c10ff, // bg
	0x0d0f14ff, // panel
	0x11141aff, // card
	0x2b3345ff, // card-border
	0x3e4c66ff, // border-hover
	0xd0d7e6ff, // text
	0x707b90ff, // muted
	0x565f70ff, // faint
	0x4a5566b3, // placeholder
	0x3a86ffff, // accent
	0x5aa0ffff, // accent-hover
	0x2f74e0ff, // accent-pressed
	0x6db0ffff, // accent-border-hover
	0x1f3550ff, // accent-soft
	0xffffffff, // on-accent
	0xffffffff, // text-strong
	0xe5484dff, // danger
	0xf06065ff, // danger-hover
	0x07090cff, // log-bg
	0xb9c4d2ff, // log-text
	0x141923ff, // popup
	0x141820ff, // tab-inactive
	0x1d212aff, // control-hover
	0x232834ff, // button-top
	0x191d26ff, // button-bottom
	0x262c38ff, // button-hover
	0x14171dff, // button-pressed
	0x8b939fff, // chrome-text
	0xffffffff, // chrome-text-hover
	0x000000a6, // scrim
};
const uint32_t kLight[] = {
	0x0000000d, // row-hover
	0x00000017, // row-selected
	0x1a2130b3, // row-outline
	0xffffffc8, // card-glass
	0xeef1f5ff, // bg
	0xe6eaf0ff, // panel
	0xffffffff, // card
	0xc5ccd8ff, // card-border
	0x8f9cb2ff, // border-hover
	0x1a2130ff, // text
	0x586377ff, // muted
	0x858fa1ff, // faint
	0x7d879ab3, // placeholder
	0x2b6fe0ff, // accent
	0x4a86eeff, // accent-hover
	0x1f5cc4ff, // accent-pressed
	0x1f5cc4ff, // accent-border-hover
	0xd4e2faff, // accent-soft
	0xffffffff, // on-accent
	0x000000ff, // text-strong
	0xd93036ff, // danger
	0xe5484dff, // danger-hover
	0xffffffff, // log-bg
	0x2a3140ff, // log-text
	0xffffffff, // popup
	0xdfe4ebff, // tab-inactive
	0xe1e6eeff, // control-hover
	0xffffffff, // button-top
	0xf0f2f6ff, // button-bottom
	0xe8ecf2ff, // button-hover
	0xdde2eaff, // button-pressed
	0x586377ff, // chrome-text
	0x1a2130ff, // chrome-text-hover
	0x00000052, // scrim
};
const uint32_t kClassic[] = {
	0xffffff0f, // row-hover
	0x3e5470cc, // row-selected
	0xd6d6d6aa, // row-outline
	0x1e1d1bc0, // card-glass
	0x2b2a28ff, // bg
	0x232220ff, // panel
	0x1e1d1bff, // card
	0x45433fff, // card-border
	0x6a6763ff, // border-hover
	0xd6d6d6ff, // text
	0x8f8f8dff, // muted
	0x6b6a67ff, // faint
	0x6b6a67b3, // placeholder
	0x5a86b8ff, // accent
	0x6f98c6ff, // accent-hover
	0x4a7099ff, // accent-pressed
	0x8fb0d6ff, // accent-border-hover
	0x3a4a5eff, // accent-soft
	0xffffffff, // on-accent
	0xffffffff, // text-strong
	0xc9453fff, // danger
	0xdb5a54ff, // danger-hover
	0x1a1a19ff, // log-bg
	0xc4c4c4ff, // log-text
	0x2f2e2cff, // popup
	0x262523ff, // tab-inactive
	0x3a3936ff, // control-hover
	0x4a4946ff, // button-top
	0x3c3b38ff, // button-bottom
	0x55534fff, // button-hover
	0x343331ff, // button-pressed
	0x8b939fff, // chrome-text
	0xffffffff, // chrome-text-hover
	0x000000a6, // scrim
};
LauncherTheme BuiltIn(const char* id, const char* name, bool light, const uint32_t (&colors)[kThemeColorCount])
{
	LauncherTheme t;
	t.id    = id;
	t.base  = id;
	t.name  = name;
	t.light = light;
	std::copy(std::begin(colors), std::end(colors), t.colors);
	return t;
}

int ColorIndex(const std::string& name)
{
	for (size_t i = 0; i < kThemeColorCount; i++)
	{
		if (name == kThemeColorNames[i])
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

// "#rrggbb" or "#rrggbbaa" as 0xRRGGBBAA.
bool ParseColor(const std::string& text, uint32_t* out)
{
	if ((text.size() != 7 && text.size() != 9) || text[0] != '#')
	{
		return false;
	}
	uint32_t v = 0;
	for (size_t i = 1; i < text.size(); i++)
	{
		const char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
		if (ch >= '0' && ch <= '9')
		{
			v = (v << 4) | static_cast<uint32_t>(ch - '0');
		} else if (ch >= 'a' && ch <= 'f')
		{
			v = (v << 4) | static_cast<uint32_t>(ch - 'a' + 10);
		} else
		{
			return false;
		}
	}
	*out = text.size() == 7 ? (v << 8) | 0xffu : v;
	return true;
}

std::string FormatColor(uint32_t c)
{
	char buf[10];
	if ((c & 0xffu) == 0xffu)
	{
		std::snprintf(buf, sizeof(buf), "#%06x", c >> 8);
	} else
	{
		std::snprintf(buf, sizeof(buf), "#%08x", c);
	}
	return buf;
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool ReadText(const fs::path& file, std::string& text)
{
	std::ifstream f(file, std::ios::binary);
	if (!f)
	{
		return false;
	}
	std::stringstream ss;
	ss << f.rdbuf();
	text = ss.str();
	// Notepad and PowerShell may add a UTF-8 BOM.
	if (text.rfind("\xEF\xBB\xBF", 0) == 0)
	{
		text.erase(0, 3);
	}
	return true;
}

} // namespace

std::vector<LauncherTheme> load_themes(const fs::path& dir, std::vector<std::string>* problems)
{
	// The order is the drop-down's, and the first three keep the indexes the launcher has always used.
	std::vector<LauncherTheme> themes = {
	    BuiltIn("dark", "Dark", false, kDark),
	    BuiltIn("light", "Light", true, kLight),
	    BuiltIn("classic", "Classic", false, kClassic),
	};
	const size_t built_in = themes.size();
	auto         problem  = [&](const fs::path& file, const std::string& what)
	{
		if (problems != nullptr)
		{
			problems->push_back(path_utf8(file.filename()) + ": " + what);
		}
	};

	std::vector<fs::path> files;
	std::error_code       ec;
	for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
	{
		if (it->is_regular_file(ec) && Lower(path_utf8(it->path().extension())) == ".json")
		{
			files.push_back(it->path());
		}
	}
	std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) { return Lower(path_utf8(a.filename())) < Lower(path_utf8(b.filename())); });

	for (const fs::path& file: files)
	{
		const std::string id = path_utf8(file.stem());
		if (std::any_of(themes.begin(), themes.end(), [&](const LauncherTheme& t) { return Lower(t.id) == Lower(id); }))
		{
			problem(file, "a theme called \"" + id + "\" already exists, so this file is skipped");
			continue;
		}
		std::string text;
		JsonValue   root;
		if (!ReadText(file, text) || !parse_json(text, root) || root.type != JsonValue::Type::Object)
		{
			problem(file, "not valid JSON, so this file is skipped");
			continue;
		}

		const auto  base_it = root.o.find("base");
		std::string base_id = kDefaultTheme;
		if (base_it != root.o.end())
		{
			base_id = base_it->second.type == JsonValue::Type::String ? Lower(base_it->second.s) : std::string();
		}
		const auto base = std::find_if(themes.begin(), themes.begin() + static_cast<std::ptrdiff_t>(built_in),
		                               [&](const LauncherTheme& t) { return t.id == base_id; });
		if (base == themes.begin() + static_cast<std::ptrdiff_t>(built_in))
		{
			problem(file, "\"base\" must be \"dark\", \"light\" or \"classic\", so this file is skipped");
			continue;
		}

		LauncherTheme t = *base;
		t.id            = id;
		t.name          = id;
		t.file          = file;
		const auto name = root.o.find("name");
		if (name != root.o.end() && name->second.type == JsonValue::Type::String && !name->second.s.empty())
		{
			t.name = name->second.s;
		}
		for (const auto& [key, value]: root.o)
		{
			if (key != "name" && key != "base" && key != "colors")
			{
				problem(file, "unknown key \"" + key + "\" ignored");
			}
		}
		const auto colors = root.o.find("colors");
		if (colors != root.o.end() && colors->second.type != JsonValue::Type::Object)
		{
			problem(file, "\"colors\" must be an object, so the base's colours are used");
		} else if (colors != root.o.end())
		{
			for (const auto& [key, value]: colors->second.o)
			{
				const int i = ColorIndex(key);
				uint32_t  c = 0;
				if (i < 0)
				{
					problem(file, "unknown colour \"" + key + "\" ignored");
				} else if (value.type != JsonValue::Type::String || !ParseColor(value.s, &c))
				{
					problem(file, "\"" + key + "\" is not #rrggbb or #rrggbbaa, so the base's colour is used");
				} else
				{
					t.colors[i] = c;
				}
			}
		}
		themes.push_back(std::move(t));
	}
	return themes;
}

int find_theme(const std::vector<LauncherTheme>& themes, const std::string& id)
{
	for (size_t i = 0; i < themes.size(); i++)
	{
		if (Lower(themes[i].id) == Lower(id))
		{
			return static_cast<int>(i);
		}
	}
	for (size_t i = 0; i < themes.size(); i++)
	{
		if (themes[i].id == kDefaultTheme)
		{
			return static_cast<int>(i);
		}
	}
	return 0;
}

bool prepare_themes_folder(const fs::path& dir, const LauncherTheme& base)
{
	std::error_code ec;
	fs::create_directories(dir, ec);
	if (!fs::is_directory(dir, ec))
	{
		return false;
	}
	for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
	{
		if (Lower(path_utf8(it->path().extension())) == ".json")
		{
			return true;
		}
	}
	std::string out = "{\n    \"name\": \"Example\",\n    \"base\": \"" + base.base + "\",\n    \"colors\": {\n";
	for (size_t i = 0; i < kThemeColorCount; i++)
	{
		out += std::string("        \"") + kThemeColorNames[i] + "\": \"" + FormatColor(base.colors[i]) + "\"" + (i + 1 < kThemeColorCount ? ",\n" : "\n");
	}
	out += "    }\n}\n";
	std::ofstream f(dir / "example.json", std::ios::binary | std::ios::trunc);
	f << out;
	return true;
}

} // namespace kyty_launcher
