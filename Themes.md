# Themes

The launcher comes with three themes: **classic** (the default), **dark** and **light**. You can add your own
by dropping a small JSON file into the `themes` folder. A theme only needs the colours you want to change. The
rest come from the built-in theme it is based on.

## Quick start

1. Open **Settings > General > Launcher** and click **Open folder** next to Theme. The launcher creates the
   `themes` folder and, if it's empty, writes `example.json`: the current theme with every colour filled in.
2. Copy `example.json` to a new name, for example `ember.json`, and change some colours.
3. Click **Reload**, then pick your theme from the Theme drop-down.

A minimal theme:

```json
{
  "name": "Ember",
  "base": "dark",
  "colors": {
    "accent": "#e0662b",
    "accent-hover": "#ec7a42",
    "accent-pressed": "#c4551f"
  }
}
```

## Where theme files go

| | Folder |
|---|---|
| Windows | `%LOCALAPPDATA%\KytyPS5\themes\` |
| Linux | `~/.local/share/kytyps5/themes/` |
| macOS | `~/Library/Application Support/KytyPS5/themes/` |
| Portable mode | `themes\` next to the launcher |

Every `*.json` file in that folder is one theme. The file name without `.json` is the theme's id, which is what
the launcher saves as your chosen theme, so renaming a file means picking it again.

## File format

All three top-level keys are optional.

| Key | What it does |
|---|---|
| `name` | The name shown in the Theme drop-down. Defaults to the file name. |
| `base` | `"dark"`, `"light"` or `"classic"` (default). Supplies every colour you leave out, and decides whether the theme counts as light or dark, which sets the scroll bars, edit boxes and hover washes. Pick the base closest to your colours. |
| `colors` | The colours to change, by key (listed below). |

Colours are written `"#rrggbb"`, or `"#rrggbbaa"` with an alpha channel (`00` is fully transparent, `ff` fully
opaque). Alpha is useful for `card-glass`, `accent-soft`, `row-hover` and `scrim`, which sit on top of other
things.

## Colour keys

**Surfaces**

| Key | Paints |
|---|---|
| `bg` | The page background |
| `panel` | The title bar and side panels |
| `card` | Cards and settings sections |
| `card-border` | Card outlines, and the window border |
| `border-hover` | An outline under the mouse |
| `card-glass` | See-through cards drawn over game art (use alpha) |
| `popup` | Menus and drop-down lists |
| `log-bg` | The console log, edit boxes and pickers |
| `scrim` | The dimming behind dialogs (use alpha) |

**Text**

| Key | Paints |
|---|---|
| `text` | Normal text |
| `text-strong` | The picked row of a drop-down |
| `muted` | Secondary text: labels, hints, comments and the search icon |
| `faint` | The dimmest text: disabled items and icons, unsupported cheats and small markers |
| `placeholder` | Placeholder text in empty edit boxes |
| `log-text` | Text in the console log and edit boxes |
| `on-accent` | Text on accent and danger fills |

**Accent and danger**

| Key | Paints |
|---|---|
| `accent` | Primary buttons, checks, the selected tab and highlights |
| `accent-hover` | `accent` under the mouse |
| `accent-pressed` | `accent` while pressed |
| `accent-border-hover` | An accent outline under the mouse |
| `accent-soft` | Banners and the selected navigation row (use alpha) |
| `danger` | Destructive buttons, such as Delete |
| `danger-hover` | `danger` under the mouse |

**Controls**

| Key | Paints |
|---|---|
| `button-top`, `button-bottom` | The top and bottom of the normal button gradient. Make them equal for a flat button |
| `button-hover` | A button under the mouse |
| `button-pressed` | A button while pressed |
| `control-hover` | The hover background of icon buttons, the console button and the key-binding boxes |
| `tab-inactive` | Tabs that aren't selected |

**Library rows**

| Key | Paints |
|---|---|
| `row-hover` | A game row under the mouse |
| `row-selected` | The selected game row |
| `row-outline` | The outline of the selected game row |

**Window chrome**

| Key | Paints |
|---|---|
| `chrome-text` | Title bar menus and the minimise, maximise and close buttons |
| `chrome-text-hover` | The same, under the mouse |

## When something is wrong

Problems are checked at start-up and after **Reload**. The status bar shows the first one and how many more there
are. The console (**View > Console**) lists every one as `warning: theme <file>: ...`.

- **Skipped files**: a file that isn't valid JSON, one with an unknown `base`, or one named `dark.json`,
  `light.json` or `classic.json` (those names belong to the built-in themes).
- **Ignored keys**: an unknown colour key or top-level key is ignored and reported. Check the spelling against
  the tables above.
- **Bad colours**: a value that isn't `#rrggbb` or `#rrggbbaa` keeps the base theme's colour.
- **Theme gone**: if the file of the theme you're using is deleted or renamed, **Reload** falls back to Classic.

## Tips

- Start from `example.json`. It has every key, so you can see what each one is set to now.
- Change a few keys at a time and click **Reload** to check. You don't need to restart the launcher.
- Check your theme on the library, Settings, Version Manager and Controls pages, and open a right-click menu and a
  dialog: between them they use every key.
- Keep enough contrast between `text` and `bg`/`card`, and between `on-accent` and `accent`.
- To share a theme, send the `.json` file. It holds nothing but colours.
