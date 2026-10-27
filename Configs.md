# Config files

The launcher reads three kinds of config file. All of them use the same setting keys, listed in
[Keys](#keys).

| File | Who writes it | What it's for |
|---|---|---|
| `cfgGlobal.json`, `cfg<TITLEID>.json` next to `kyty_emulator.exe` | the person who publishes a build | settings a build needs to work |
| `kyty.json`, `configs/games/<TITLEID>.json` in the launcher's config folder | the launcher, from the Settings page | the user's own global and per-game settings |
| A game config export (any name, `.json`) | **Settings > Games > Export config / Import configs** | sharing a game's settings, same format as the Qt launcher |

Most of this page is for build makers. [Where files live](README.md#where-files-live) in the README says where
the config folder is on each platform.

## Build cfg files

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
- The user's per-game files use the same `"emu"` block, so keys copy across as they are (see below).

## The user's per-game files

The launcher writes `configs/games/<TITLEID>.json` when someone saves a game's settings, and removes the file
again when nothing is left in it. It holds only what differs from the global settings:

```json
{
  "version": 1,
  "emulator": "C:\\Users\\you\\AppData\\Local\\KytyPS5\\versions\\KytyPS5-v0.9\\kyty_emulator.exe",
  "emu": {
    "present_mode": "Immediate"
  }
}
```

- `"emu"` is the same block as in a build cfg file.
- `"emulator"` is the game's build pin (**Settings > Games**). Build cfg files ignore this key.
- Editing it by hand works while the launcher is closed. A file with a `"version"` higher than 1 is left alone.

## Importing and exporting game configs

**Settings > Games** can export one game's config and import a file holding many. The format is the upstream Qt
launcher's, so files move between the two launchers:

```json
{
  "PPSA01234": {
    "present_mode": "Immediate",
    "vblank_frequency": "60",
    "hdr_enabled": "true"
  }
}
```

- Top-level keys are title IDs. Entries that aren't a serial are skipped.
- Values are usually strings, as the Qt launcher stores them, but JSON `true`/`false` and numbers are accepted
  too. Booleans also take `"1"`/`"0"`. `screen_resolution` takes `"R1920X1080"` or `"1920x1080"`.
- `env` and `host_input_mapping` aren't part of this format and are skipped.
- An import shows a summary first, including any value it couldn't read. It stores only values that differ from
  the global settings, and keeps the game's other settings.
- An export writes every value the game starts with, including anything the build's cfg files set.
