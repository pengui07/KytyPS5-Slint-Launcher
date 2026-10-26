# Feature ideas — for review (not implemented)

Low-risk fixes were applied directly (see "Applied" below).
The items here are proposals only, grouped by risk. Nothing here is coded.

## Applied during this session (no review needed, all low-risk)
- AppSlider hover-drag: `moved` now requires `pressed`; thumb hover/pressed tint; disabled dim.
- Settings vibration/volume sliders replaced with SpinBoxes (AppSlider thumb/value never painted in settings context despite six layout variants; SpinBox paints with values — verified "100"/"100"). Controller vibration/volume Slint properties changed float→int to match C++ (updated set/get call sites). Settings General left column fixed at 700px so the right column stays on-screen.
- Controls capture moved from pointer-`down` to `clicked` (drag-scroll no longer traps you in capture); tabs lock while listening; capture records which tab started it (`capture-pad`) so the pad dropdown can't mis-assign keyboard rows; sensitivity write-back skips no-change drags; restore-defaults only refreshes sensitivity on the keyboard tab.
- Selector (dropdown) popup: capped at ~10 rows with its own scroll + min width (23 gamepad sources overflowed short windows).
- Toolbar: unified with the dark theme (was the only `#303030` light surface); VM button reachable.
- Version Manager / Trophies / Controls share one header pattern (title + back); trophies grades moved to their own row.
- **Layout root cause fixed**: `horizontal-stretch` inside an unbounded layout (ScrollView content, bare page layouts) evaluates to infinity and pushes everything after it off-screen (buttons painted nothing but still received clicks). Removed stretch-before-siblings in Controls rows, tab strip, action rows, toolbar spacers; gave ScrollViews explicit content widths; explicitly sized overlay page layouts.
- Controls Save/Cancel/Restore moved to just under the tabs (bottom-anchored actions were pushed off-screen with no way to reach them).
- Fixed-width action/back buttons (default grow-to-fill ballooned them fullscreen at wide sizes).
- Tab label "Keyboard & Mouse" truncated — widened TabHeader 140→170px (verify visually).
- Touchpad card moved above the list + sensitivity row moved above the list (below-list content gets pushed off with no page scroll); lists bounded (gamepad max-height, keyboard fixed); manual Up/Down scroll buttons in the actions row step content-y via globals (wheel/drag/scrollbar don't reach Slint Flickable here — verified D-pad rows scroll into view).
- Play/Settings/VM capped with explicit leaf widths (verified compact at 2062px). Note: Slint forbids width+min/max-width on the same element (compile error), and width below the component min-width suppresses paint.
- Settings back button restored (was pushed off by a stretch spacer, trapping users); tab buttons fixed at 110px so Games tab stays reachable (it was pushed off-screen).

## Low risk (polish, no behavior change)
- [ ] Single `PageHeader` component (deleted — inline headers proved more reliable; revisit if Slint fixes unbounded stretch).
- [ ] Unify selection (`row-selected/outline`) and containers (single `Card`).
- [ ] Slider keyboard/focus support.
- [ ] Deduplicate menus: View › Page duplicates toolbar; Help › Check-for-updates is a dead placeholder; "Game settings…" appears in 3 places.
- [ ] Spot-check on real hardware: AppSlider drag feel on sensitivity/vibration/volume rows (paint verified, drag path fixed in code), list Up/Down repeat feel.
- Settings all two-column tabs (General/Graphics/Advanced/Debug) left columns fixed at 700px so right columns stay on-screen at wide windows.
- [ ] Play/Settings balloon at wide windows (default grow-to-fill balloons them; `stretch: 0` did not constrain — needs explicit widths).
- [ ] Remove-game (trash icon) has NO confirmation — single click hides the game (recoverable via Settings > Games > Restore, files untouched, but scary). Recommend a confirm dialog.
- [ ] Library bottom status bar can be cut off at short window heights (vertical overflow; same stretch family).
- [ ] Window geometry persists (87,258 1200x560) — good; preferred 1200x780 vs min 1000x560 sane.

## Medium risk (layout reshuffle, needs your call)
- [ ] Balance Settings tabs: Graphics is 1 Card + empty column; Advanced mixes Launch + Env + GitHub token + Release cache. Proposal: fill Graphics column, move token/cache into Version Manager, make Games removed-list collapsible.
- [ ] Microphone custom dropdown (`settings.slint`) → reuse `Selector`.
- [ ] GPU picker: single `FormRow` whose model swaps, instead of swapping whole rows (avoids layout jump).
- [ ] Disable log-path fields when log mode is Silent.
- [ ] Preserve controls list scroll offset across `refresh_controls()`.

## Higher risk / new features (do not start without approval)
- [ ] Grid/list view toggle persistence + per-game art cache settings UI.
- [ ] Version Manager auto-check scheduling UI.
- [ ] Trophy sync / comparison view.
- [ ] First-run import wizard improvements (Qt `.ini` preview before import).
- [ ] Playtime reset/edit UI.

## Reference notes (from your list)
- PCSX2 / RPCS3: dense settings with left tree + right page; consistent 26–28px controls, grouped cards.
- Classic Steam client: dark panels, 1px borders, small caps section labels.
- qBittorrent: compact tables, persistent sort + filter bar.
- shadPS4: popup menu shaping mimicked via `slint-style/fluent-menu.slint` + stamp/hash rebuild guard in `CMakeLists.txt`.

## Verification log
- Library (1214 + maximized): table, sort, art, builds all good. NHL was accidentally hidden by a test mis-click and restored via config (data + playtime intact; favourite intact).
- Controls gamepad + keyboard tabs: all bindings paint with values (A/Cross, J/K/L…, Up/Down…); capture dialog opens with correct label.
- Version Manager: builds list, pins, search, dropdown all render.
- Trophies: titles, grades, game rows, progress bars, empty state all render.
- Settings General tab: profile, audio, paths, system checkboxes all render with values.
- Builds to test on demand: GTA with `GTA-Latest`, NHL with `hairfix` (per your note). Not launched this session (UI work only).
