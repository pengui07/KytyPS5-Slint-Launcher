#include "platform.h"

#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace kyty_launcher {

namespace fs = std::filesystem;

#ifdef _WIN32

void open_in_system(const std::string& target)
{
	ShellExecuteW(nullptr, L"open", utf8_path(target).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string run_capture(const std::vector<std::string>&, bool* ok)
{
	if (ok != nullptr)
	{
		*ok = false;
	}
	return {};
}

bool detect_primary_monitor(int* width, int* height)
{
	// The real pixel size; GetSystemMetrics would report a scaled-down one for a process that isn't DPI aware.
	DEVMODEW mode {};
	mode.dmSize = sizeof(mode);
	if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) == FALSE || mode.dmPelsWidth == 0 || mode.dmPelsHeight == 0)
	{
		return false;
	}
	*width  = static_cast<int>(mode.dmPelsWidth);
	*height = static_cast<int>(mode.dmPelsHeight);
	return true;
}

std::string pick_path_dialog(const std::string&, bool)
{
	return {};
}

#else

namespace {

std::string Trim(std::string s)
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0)
	{
		s.pop_back();
	}
	size_t b = 0;
	while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0)
	{
		b++;
	}
	return s.substr(b);
}

// Whether `name` is an executable in one of the PATH folders.
bool ProgramExists(const std::string& name)
{
	const char* path = std::getenv("PATH");
	if (path == nullptr)
	{
		return false;
	}
	const std::string all = path;
	for (size_t pos = 0; pos <= all.size();)
	{
		size_t end = all.find(':', pos);
		if (end == std::string::npos)
		{
			end = all.size();
		}
		const std::string dir = all.substr(pos, end - pos);
		if (!dir.empty() && access((dir + "/" + name).c_str(), X_OK) == 0)
		{
			return true;
		}
		pos = end + 1;
	}
	return false;
}

} // namespace

std::string run_capture(const std::vector<std::string>& args, bool* ok)
{
	if (ok != nullptr)
	{
		*ok = false;
	}
	if (args.empty())
	{
		return {};
	}
	std::vector<std::string> copy = args;
	std::vector<char*>       argv;
	for (auto& a: copy)
	{
		argv.push_back(a.data());
	}
	argv.push_back(nullptr);

	int fds[2];
	if (pipe(fds) != 0)
	{
		return {};
	}
	pid_t pid = fork();
	if (pid < 0)
	{
		close(fds[0]);
		close(fds[1]);
		return {};
	}
	if (pid == 0)
	{
		dup2(fds[1], 1);
		close(fds[0]);
		close(fds[1]);
		execvp(argv[0], argv.data());
		_exit(127);
	}
	close(fds[1]);
	std::string out;
	char        buf[4096];
	ssize_t     got = 0;
	while ((got = read(fds[0], buf, sizeof(buf))) > 0)
	{
		out.append(buf, static_cast<size_t>(got));
	}
	close(fds[0]);
	int status = 0;
	waitpid(pid, &status, 0);
	if (ok != nullptr)
	{
		*ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}
	return Trim(out);
}

void open_in_system(const std::string& target)
{
	// Detached: the opener may run for as long as the user keeps the document open.
	pid_t pid = fork();
	if (pid == 0)
	{
		if (fork() != 0)
		{
			_exit(0);
		}
#ifdef __APPLE__
		execlp("open", "open", target.c_str(), static_cast<char*>(nullptr));
#else
		execlp("xdg-open", "xdg-open", target.c_str(), static_cast<char*>(nullptr));
#endif
		_exit(127);
	}
	if (pid > 0)
	{
		int status = 0;
		waitpid(pid, &status, 0);
	}
}

std::string pick_path_dialog(const std::string& current, bool folder)
{
	// Start in the folder of the current value when it exists.
	std::error_code ec;
	fs::path        start = utf8_path(current);
	if (!fs::is_directory(start, ec))
	{
		start = start.parent_path();
	}
	if (start.empty() || !fs::is_directory(start, ec))
	{
		const char* home = std::getenv("HOME");
		start            = home != nullptr ? home : "/";
	}
	const std::string dir = path_utf8(start);

	bool ok = false;
#ifdef __APPLE__
	const std::string script = std::string("POSIX path of (choose ") + (folder ? "folder" : "file") +
	                           " default location POSIX file \"" + dir + "\")";
	std::string picked = run_capture({"osascript", "-e", script}, &ok);
	if (ok && !picked.empty())
	{
		return picked;
	}
	return {};
#else
	if (ProgramExists("zenity"))
	{
		std::vector<std::string> zenity = {"zenity", "--file-selection", "--filename=" + dir + "/"};
		if (folder)
		{
			zenity.push_back("--directory");
		}
		return run_capture(zenity, &ok);
	}
	if (ProgramExists("kdialog"))
	{
		return run_capture({"kdialog", folder ? "--getexistingdirectory" : "--getopenfilename", dir}, &ok);
	}
	return {};
#endif
}

#ifndef __APPLE__
namespace {

// "2560x1440+0+0" (the geometry xrandr prints for an active output) -> 2560, 1440.
bool ParseGeometry(const std::string& token, int* width, int* height)
{
	int w = 0;
	int h = 0;
	int x = 0;
	int y = 0;
	char tail = 0;
	// The offsets must follow, so a mode list entry like "1920x1080" alone is not taken.
	if (std::sscanf(token.c_str(), "%dx%d+%d+%d%c", &w, &h, &x, &y, &tail) == 4 && w > 0 && h > 0)
	{
		*width  = w;
		*height = h;
		return true;
	}
	return false;
}

} // namespace
#endif

bool detect_primary_monitor(int* width, int* height)
{
#ifdef __APPLE__
	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
	if (mode == nullptr)
	{
		return false;
	}
	// Pixels, not the points of a scaled "looks like" mode.
	const size_t w = CGDisplayModeGetPixelWidth(mode);
	const size_t h = CGDisplayModeGetPixelHeight(mode);
	CGDisplayModeRelease(mode);
	if (w == 0 || h == 0)
	{
		return false;
	}
	*width  = static_cast<int>(w);
	*height = static_cast<int>(h);
	return true;
#else
	// X11 and XWayland. Wayland-only sessions without xrandr answer nothing, which is fine: no recommendation.
	if (!ProgramExists("xrandr"))
	{
		return false;
	}
	bool              ok  = false;
	const std::string out = run_capture({"xrandr", "--current"}, &ok);
	if (!ok)
	{
		return false;
	}
	bool found = false;
	for (size_t pos = 0; pos < out.size();)
	{
		size_t            eol  = out.find('\n', pos);
		const std::string line = out.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
		pos                    = eol == std::string::npos ? out.size() : eol + 1;
		if (line.find(" connected") == std::string::npos)
		{
			continue;
		}
		// "DP-1 connected primary 2560x1440+0+0 (...)" or "HDMI-1 connected 1920x1080+2560+0 (...)"
		const bool primary = line.find(" primary ") != std::string::npos;
		int        w       = 0;
		int        h       = 0;
		for (size_t start = 0; start < line.size();)
		{
			size_t end = line.find(' ', start);
			if (end == std::string::npos)
			{
				end = line.size();
			}
			if (ParseGeometry(line.substr(start, end - start), &w, &h))
			{
				break;
			}
			start = end + 1;
		}
		if (w == 0)
		{
			continue; // connected but not active
		}
		if (!found || primary)
		{
			*width  = w;
			*height = h;
			found   = true;
		}
		if (primary)
		{
			break;
		}
	}
	return found;
#endif
}

#endif

} // namespace kyty_launcher
