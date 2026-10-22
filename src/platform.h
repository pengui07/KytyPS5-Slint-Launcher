#pragma once

#include <string>
#include <vector>

namespace kyty_launcher {

// Opens a folder, file or link with the system's default handler (Explorer / Finder / xdg-open).
void open_in_system(const std::string& target);

// Runs a program without a shell and returns what it printed (stdout), trimmed; `ok` says whether
// it started and exited with 0. Used for the file dialogs of the desktop on macOS and Linux.
std::string run_capture(const std::vector<std::string>& args, bool* ok);

// A folder or file picked in the desktop's own dialog (osascript on macOS, zenity or kdialog on
// Linux); "" when cancelled or when no dialog program is installed.
std::string pick_path_dialog(const std::string& current, bool folder);

// The pixel size of the primary monitor (Win32 on Windows, CoreGraphics on macOS, `xrandr` on Linux).
// False when it can't be told (for example on Wayland without XWayland); the outputs are then unchanged.
bool detect_primary_monitor(int* width, int* height);

} // namespace kyty_launcher
