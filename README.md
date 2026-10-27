# KytyPS5 Slint Launcher

A fast, single-exe launcher for the [KytyPS5](https://github.com/KytyPS5/KytyPS5) emulator, written in C++ with
[Slint](https://slint.dev). It replaces the Qt launcher and reads its `Kyty.ini`, saves, trophies and cheats, so you
can switch over without losing anything.

- [Features](#features)
- [Getting started](#getting-started)
- [Help and troubleshooting](#help-and-troubleshooting)
- [Guide for build makers: cfg files](#guide-for-build-makers-cfg-files)
- [Building the launcher](#building-the-launcher)

## Features

**Library**
- Scans one or more game folders for games (any folder with an `eboot.bin`) and reads each game's
  `sce_sys/param.json` for its name, title ID, version and icon. A `.zar` archive added as a game is listed too
  (by file name, since the launcher cannot look inside it).
- List and grid views, sortable columns (sort is remembered) and "Pin to top" favourites.
- Play time and "last played" per game, kept in `playtime.json`, so hiding or rescanning a game keeps its totals.
- Compatibility status from the shared KytyPS5 compatibility list.
- Right-click menu per game: Run, Trophies, Cheats, Open game folder, Open save folder, Game
  settings, Pin to top, Remove from list. Delete save data asks first and wipes only that game's saves. Removed games can be brought back from **Settings > Games**.

**Settings**
- Every emulator option the current KytyPS5 understands, on four tabs: **General** (profile, language, audio,
  paths, controller, trophy popups), **Graphics**, **Advanced** (extra arguments, environment variables, GitHub
  token, release cache) and **Debug**.
- **Per-game settings**: open Game settings on any game to change options for that game only. The page shows what
  the game actually resolves to, including anything the emulator build itself forces (see the cfg guide below).
- **Per-game emulator pin**: run a specific game with a specific build (**Settings > Games**).
- Three themes: dark, light and classic.

**Version Manager**
- Download KytyPS5 releases from GitHub, keep several side by side, and switch with one click from the toolbar.
- Add other GitHub repositories as extra release sources (each gets its own tab), add local builds with Add
  Custom, rename, pin, hide and delete builds.
- Checks for newer `-latest` builds at start-up and hourly (can be turned off).
- Each build's About panel lists the cfg files it ships with.
- An optional GitHub token (stored in Windows Credential Manager, never in the settings file) lifts GitHub's rate
  limit and allows private repositories.

**Controls**
- Remap the keyboard, mouse and gamepad for every PS5 control, with a controller diagram, capture-by-pressing on
  Windows, touch pad options and mouse sensitivity. Saved in the format the emulator's `--keymap` reads.

**Cheats**
- Per-game cheat list, compatible with the Qt launcher's GoldHEN-style JSON files in `<run folder>/_Patches/`.
- Browse and import cheats from the online HEN Cheats Collection; files that don't match the installed game
  version or use features the emulator can't apply yet are refused with the reason.

**Trophies**
- Trophy overview of all games and a page per game with icons, grades and unlock state, in your console language.

**Everything else**
- First-start setup: keep data in your user folder or next to the launcher (portable), choose where saves go, and
  import the Qt launcher's settings and games in one step.
- Console window with the emulator's live output and the full last log.
- `--dry-run` and `--play` command-line modes (see below).

## Getting started

1. Download the launcher and start it. The first-start window asks two things:
   - **Launcher data**: *App data* (recommended) or *Next to the launcher* (portable).
   - **Saves**: keep using the Qt launcher's folder, keep them with the launcher data, or pick another folder.
     If a Qt launcher `Kyty.ini` was found you can import its settings and game list here.
2. Open the **Version Manager** and download a build, or use **Add Custom** to point at a `kyty_emulator` you
   already have.
3. In **Settings > General**, add your game folders. Your games appear in the library; double-click one to play.

### Where files live

| | App data (Windows) | Portable | Linux | macOS |
|---|---|---|---|---|
| Settings `kyty.json` | `%LOCALAPPDATA%\KytyPS5\configs\` | `<launcher>\configs\` | `~/.config/kytyps5/` | `~/Library/Application Support/KytyPS5/configs/` |
| Per-game settings | `configs\games\<TITLEID>.json` | same | `~/.config/kytyps5/games/` | same pattern |
| Builds, cache, logs | `%LOCALAPPDATA%\KytyPS5\versions`, `cache`, `logs` | `<launcher>\...` | `~/.local/share/kytyps5/` | `~/Library/Application Support/KytyPS5/` |
| Saves, trophies, cheats | `<run folder>\_SaveData`, `_Trophies`, `_Patches` | | | |

Portable mode is on whenever a file named `portable.txt` sits next to the launcher exe. On Linux the
`XDG_CONFIG_HOME` and `XDG_DATA_HOME` variables are honoured.

## Help and troubleshooting

**First stop: the Console and the last log.** Open **View > Console**. It shows the emulator's output live, and
*Last log* loads the full output of the previous run from `logs/last-run.log`.

**See exactly what will be launched.** Run the launcher from a terminal with `--dry-run`:

```
kyty_slint_launcher.exe --dry-run            # print to the terminal
kyty_slint_launcher.exe --dry-run plan.txt   # or write it to a file
```

It prints the config layers that were applied (and how many keys each one set), the exe, working folder, full
command line and environment, plus any warnings. This is the best thing to paste into a bug report.
`--play` launches the last selected game without a window.

**Common problems**

- **"A setting I changed is ignored."** The selected build probably ships a `cfgGlobal.json` or
  `cfg<TITLEID>.json` that sets that option. Build cfg files outrank your global settings. The console and
  `--dry-run` print a warning such as *this build's config files set vblank_frequency over the saved value*.
  To override it for one game, change it in that game's **Game settings** (per-game settings win over build
  files), or use a different build.
- **"Pinned emulator ... is missing, using default."** The build a game was pinned to was deleted or moved.
  Re-pin the game in **Settings > Games**.
- **A game is missing from the library.** Check that its folder is under one of your game folders and contains
  `eboot.bin`. If you removed it from the list, restore it in **Settings > Games**.
- **A game can't have its own settings / cheats / trophies.** Those are keyed by the title ID in
  `sce_sys/param.json`. Games without one (and `.zar` archives) can't have them. Cheats are only for PS5 titles
  (`PPSA` + 5 digits).
- **A cheat file is refused.** The online file must match the game's title ID, installed version and executable,
  and use only plain memory writes. Master codes and module-specific cheats aren't supported by the emulator yet.
- **Version Manager says GitHub's rate limit was hit.** Add a GitHub token in **Settings > Advanced**, or set
  `GITHUB_TOKEN` / `GH_TOKEN`. Without network the last fetched release list is shown with its age.
- **Settings didn't load: "is from a newer launcher".** `kyty.json` was written by a newer launcher. This one
  uses defaults and won't overwrite the file. Update the launcher.
- **Moving from the Qt launcher later.** Use **File > Import Kyty.ini...**. Your `Kyty.ini` is never changed.

When reporting a bug, include the `--dry-run` output, `logs/last-run.log`, the build name from the Version
Manager, and the game's title ID.

## Guide for build makers: cfg files

If you publish a KytyPS5 build that needs particular settings (a fix that only works with `Immediate` present
mode, a hack that needs PlayGo off, and so on), ship **cfg files** next to `kyty_emulator.exe` in your release zip.
The launcher reads them every time it launches a game with that build, so users don't have to change anything.

### File names and where they go

```
MyBuild/
├── kyty_emulator.exe
├── cfgGlobal.json        # applies to every game run with this build
└── cfgPPSA01234.json     # applies only to the game with title ID PPSA01234
```

- `cfgGlobal.json` for settings the build always needs.
- `cfg<TITLEID>.json` for one game. The title ID is the `titleId` in the game's `sce_sys/param.json`, for
  example `PPSA01234`. Match its case exactly: Linux and macOS file names are case-sensitive.
- The files must be in the same folder as the emulator exe. The Version Manager's About panel lists the ones it
  found, which is a quick way to check your zip.

### Format

```jsonc
{
  "version": 1,
  "emu": {
    // Comments starting with // are allowed.
    "present_mode": "Immediate",
    "vblank_frequency": 60,
    "playgo_hack_enabled": true,
    "env": { "MY_BUILD_DEBUG": "1" }
  }
}
```

- Only keys inside `"emu"` are read. Anything else in the file is ignored.
- `"version"` is optional and means 1. A file with a higher version is skipped entirely.
- Put in only the keys your build needs. Every key you leave out keeps the user's own value.
- A key with the wrong JSON type (`"60"` instead of `60`, `"true"` instead of `true`) is silently ignored.
- Enum values are case-sensitive (`"Mailbox"`, not `"mailbox"`).
- A UTF-8 BOM is fine. Trailing commas are not.

### How the layers combine

For each launch the launcher starts from the user's settings and applies, in order (later wins):

1. Built-in defaults
2. The user's global settings (`kyty.json`)
3. **`cfgGlobal.json`** of the build that will run (the game's pinned build if it has one)
4. **`cfg<TITLEID>.json`** of that build
5. The user's per-game settings (`configs/games/<TITLEID>.json`)

So your files override the user's global choices, but a user can still override them for one game. Nothing is
written back: switch to another build and the user's own values return. When your file changes a value the user
had set, the launcher warns about it in the console and in `--dry-run`, so users can tell why a setting "doesn't
stick".

To test your files, select your build, select a game and run `kyty_slint_launcher --dry-run`. Each layer is listed as
`applied, N key(s) set: ...`, `not found`, `unreadable, ignored` or `newer version N, ignored`.

### Keys

Each key maps to an emulator command-line option.

**Display and graphics**

| Key | Type | Values / default |
|---|---|---|
| `screen_resolution` | string | `"R<width>X<height>"`, e.g. `"R1920X1080"` (capital R and X). Default `"R2560X1440"` |
| `present_mode` | string | `"Fifo"`, `"Mailbox"` (default), `"Immediate"` |
| `vblank_frequency` | number | 30 to 360, default 60 |
| `gpu_index` | number | -1 = automatic (default), 0, 1, ... |
| `shader_optimization_type` | string | `"None"` (default), `"Size"`, `"Performance"` |
| `fullscreen_enabled` | bool | false |
| `hdr_enabled` | bool | false |
| `tessellation_enabled` | bool | false |
| `occlusion_queries_enabled` | bool | false |
| `shader_cache_enabled` | bool | true |
| `readback_linear_images` | bool | false |
| `readback_tiled_images` | bool | false |
| `sync_raw_image_buffers` | bool | false |

**General**

| Key | Type | Values / default |
|---|---|---|
| `user_name` | string | `"Kyty"` |
| `user_id` | number | 1000 |
| `console_language` | number | 0 to 29, default 1 (English, United States) |
| `audio_input_device` | string | microphone name, `""` = none |
| `hide_cursor_enabled` | bool | false |
| `perf_overlay_enabled` | bool | false |
| `skip_notice_screen` | bool | false |
| `network_connected` | bool | false |
| `splash_enabled` | bool | true |
| `trophy_enabled` | bool | true (trophy popups) |
| `trophy_duration_seconds` | number | 5 to 10, default 6 |
| `amd_cpu_enabled` | bool | false |
| `red_zone_protection_enabled` | bool | false |
| `vr_enabled` | bool | false |
| `playgo_hack_enabled` | bool | false |
| `extra_args` | string | extra command-line text appended as is, default `""` |
| `env` | object | `{"NAME": "value"}` environment for the emulator only. Merged by name with the user's table; `""` removes the variable |

**Controller**

| Key | Type | Values / default |
|---|---|---|
| `controller_color` | string | lightbar `"#rrggbb"`, `""` = default |
| `controller_vibration_intensity` | number | 0 to 100, default 100 |
| `controller_speaker_volume` | number | 0 to 100, default 50 |
| `controller_keyboard_player` | number | 0 = shared with player 1 (default), 1 to 4 |
| `host_input_mapping` | array of strings | `--keymap` entries such as `"Cross=J"`, `"Pad:Cross=South"`, `"Keyboard=Disabled"`. Replaces the whole list, so only set it if your build really needs a layout |

**Debug and logging**

| Key | Type | Values / default |
|---|---|---|
| `shader_log_direction` | string | `"Silent"` (default), `"Console"`, `"File"` |
| `shader_log_folder` | string | `""` = emulator default |
| `printf_direction` | string | `"Silent"` (default), `"Console"`, `"File"` |
| `printf_output_file` | string | `""` = emulator default |
| `command_buffer_dump_enabled` | bool | false |
| `command_buffer_dump_folder` | string | `""` = emulator default |
| `vulkan_validation_enabled` | bool | false |
| `shader_validation_enabled` | bool | false |
| `gpu_assisted_validation_enabled` | bool | false |
| `spirv_debug_printf_enabled` | bool | false |
| `graphics_debug_dump_enabled` | bool | false |
| `renderdoc_enabled` | bool | false |
| `profiler_enabled` | bool | false |

The key names follow the Qt launcher's `Kyty.ini` `[GlobalConfiguration]` section. The full list lives in
`ReadEmu` in [`src/settings.cpp`](src/settings.cpp).

### Tips

- Keep cfg files small. Forcing `screen_resolution` or `fullscreen_enabled` overrides what users picked for their
  own monitor, so only do it when the build truly needs it.
- Prefer a per-game `cfg<TITLEID>.json` over `cfgGlobal.json` when a fix is for one game.
- The per-game settings files the launcher writes (`configs/games/<TITLEID>.json`) use the same format, so you can
  copy keys between them. They may also hold `"emulator"` (the pinned build), which build cfg files ignore.

## Building the launcher

Requirements:
- CMake >= 3.21, Ninja (or a Visual Studio generator)
- A C++20 compiler: MSVC, clang-cl, GCC or Clang
- Rust >= 1.92 (Slint is built from source)
- Linux: libcurl and libfontconfig1 dev packages

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, run these from an "x64 Native Tools Command Prompt for VS". The exe ends up in `build/`. On Windows it
links the static VC++ runtime, so it needs no redistributable (turn off `KYTY_SLINT_STATIC_CRT` to change that).
