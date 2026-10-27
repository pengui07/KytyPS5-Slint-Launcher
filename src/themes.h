#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace kyty_launcher {

// Launcher colour themes: the three built in (dark, light, classic) and any *.json file in <data root>/themes.
// A file names a built-in theme as its base and changes only the colours it lists:
//   { "name": "Ember", "base": "dark", "colors": { "accent": "#e0662b", "bg": "#120d0a" } }
// Colours are "#rrggbb" or "#rrggbbaa". The base also decides whether the theme is light, which picks the
// scroll bars, edit boxes and the see-through hover wash. The file name without .json is the theme's id, the
// value the "theme" setting keeps.

// Every colour of ui/common.slint's Theme global: X(setter_name, "json-name"), in the same order.
#define KYTY_THEME_COLORS(X) \
	X(row_hover, "row-hover") \
	X(row_selected, "row-selected") \
	X(row_outline, "row-outline") \
	X(card_glass, "card-glass") \
	X(bg, "bg") \
	X(panel, "panel") \
	X(card, "card") \
	X(card_border, "card-border") \
	X(border_hover, "border-hover") \
	X(text, "text") \
	X(muted, "muted") \
	X(faint, "faint") \
	X(placeholder, "placeholder") \
	X(accent, "accent") \
	X(accent_hover, "accent-hover") \
	X(accent_pressed, "accent-pressed") \
	X(accent_border_hover, "accent-border-hover") \
	X(accent_soft, "accent-soft") \
	X(on_accent, "on-accent") \
	X(text_strong, "text-strong") \
	X(danger, "danger") \
	X(danger_hover, "danger-hover") \
	X(log_bg, "log-bg") \
	X(log_text, "log-text") \
	X(popup, "popup") \
	X(tab_inactive, "tab-inactive") \
	X(control_hover, "control-hover") \
	X(button_top, "button-top") \
	X(button_bottom, "button-bottom") \
	X(button_hover, "button-hover") \
	X(button_pressed, "button-pressed") \
	X(chrome_text, "chrome-text") \
	X(chrome_text_hover, "chrome-text-hover") \
	X(scrim, "scrim")

inline constexpr const char* kThemeColorNames[] = {
#define KYTY_THEME_NAME(id, name) name,
	KYTY_THEME_COLORS(KYTY_THEME_NAME)
#undef KYTY_THEME_NAME
};
inline constexpr size_t kThemeColorCount = sizeof(kThemeColorNames) / sizeof(kThemeColorNames[0]);

// What a new install starts with, and what a missing or deleted theme falls back to.
inline constexpr const char* kDefaultTheme = "classic";

struct LauncherTheme
{
	std::string           id;    // "dark", "light", "classic", or a file's name without .json
	std::string           base;  // the built-in theme it starts from (a built-in one's own id)
	std::string           name;  // shown in the Theme drop-down
	bool                  light = false;
	uint32_t              colors[kThemeColorCount] = {}; // 0xRRGGBBAA, in kThemeColorNames order
	std::filesystem::path file;  // empty for the built-in ones
};

// The built-in themes, then the files in `dir` sorted by name. A file that can't be read, has an unknown base or
// takes a built-in theme's id is left out; a colour that can't be read keeps the base's. Each such problem adds
// one line to `problems` (if given).
std::vector<LauncherTheme> load_themes(const std::filesystem::path& dir, std::vector<std::string>* problems = nullptr);

// The index of the theme with this id, else the default theme's.
int find_theme(const std::vector<LauncherTheme>& themes, const std::string& id);

// Creates `dir` and, when it holds no theme file yet, writes example.json: every colour of `base`, so a copy can be
// edited into a new theme. Returns false when the folder can't be created.
bool prepare_themes_folder(const std::filesystem::path& dir, const LauncherTheme& base);

} // namespace kyty_launcher
