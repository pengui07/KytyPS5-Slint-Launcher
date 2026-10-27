#include "self_update.h"

#include "net.h"
#include "settings.h"
#include "versions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef KYTY_LAUNCHER_VERSION
#define KYTY_LAUNCHER_VERSION ""
#endif

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

// The archive a release carries for this platform, by the end of its name.
#if defined(_WIN32)
constexpr const char* kAssetSuffix = "-Windows-x64.zip";
#elif defined(__linux__) && defined(__x86_64__)
constexpr const char* kAssetSuffix = "-Linux-x86_64.tar.gz";
#else
constexpr const char* kAssetSuffix = "";
#endif

// The work folder next to the launcher; a leftover one (a crash mid-update) is deleted at the next start.
constexpr const char* kWorkDir = ".kyty-update";

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool EndsWith(const std::string& s, const std::string& tail)
{
	return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// "v1.10.2" -> {1, 10, 2}; anything that isn't a digit separates numbers.
std::vector<long> Numbers(const std::string& tag)
{
	std::vector<long> out;
	for (size_t i = 0; i < tag.size();)
	{
		if (std::isdigit(static_cast<unsigned char>(tag[i])) == 0)
		{
			i++;
			continue;
		}
		size_t j = i;
		while (j < tag.size() && std::isdigit(static_cast<unsigned char>(tag[j])) != 0)
		{
			j++;
		}
		out.push_back(std::strtol(tag.substr(i, std::min<size_t>(j - i, 9)).c_str(), nullptr, 10));
		i = j;
	}
	return out;
}

const JsonValue* Field(const JsonValue& obj, const char* key, JsonValue::Type type)
{
	auto it = obj.o.find(key);
	return it != obj.o.end() && it->second.type == type ? &it->second : nullptr;
}

std::string Text(const JsonValue& obj, const char* key)
{
	const JsonValue* v = Field(obj, key, JsonValue::Type::String);
	return v != nullptr ? v->s : std::string();
}

bool Fail(std::string* error, const std::string& why)
{
	if (error != nullptr)
	{
		*error = why;
	}
	return false;
}

// The file the launcher runs from inside an unpacked release: the one with the running exe's name, at the top or
// one folder down (an archive may wrap its files in a folder).
fs::path FindPayloadRoot(const fs::path& unpacked, const std::string& exe_name)
{
	std::error_code ec;
	if (fs::is_regular_file(unpacked / exe_name, ec))
	{
		return unpacked;
	}
	for (fs::directory_iterator it(unpacked, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code ie;
		if (it->is_directory(ie) && fs::is_regular_file(it->path() / exe_name, ie))
		{
			return it->path();
		}
	}
	return {};
}

} // namespace

std::string launcher_version()
{
	return KYTY_LAUNCHER_VERSION;
}

bool launcher_updates_supported()
{
	return !launcher_version().empty() && std::string(kAssetSuffix).size() != 0;
}

int compare_launcher_versions(const std::string& a, const std::string& b)
{
	const std::vector<long> x = Numbers(a);
	const std::vector<long> y = Numbers(b);
	if (x.empty() || y.empty())
	{
		return x.empty() && !y.empty() ? -1 : 0;
	}
	for (size_t i = 0; i < std::max(x.size(), y.size()); i++)
	{
		const long p = i < x.size() ? x[i] : 0;
		const long q = i < y.size() ? y[i] : 0;
		if (p != q)
		{
			return p < q ? -1 : 1;
		}
	}
	return 0;
}

bool fetch_latest_launcher_release(LauncherRelease& out, std::string* error)
{
	std::string body;
	std::string net_error;
	const std::string url = std::string("https://api.github.com/repos/") + kLauncherRepo + "/releases/latest";
	if (!http_get(
	        url,
	        [&](const char* p, size_t n)
	        {
		        body.append(p, n);
		        return body.size() < 4u * 1024u * 1024u;
	        },
	        nullptr, &net_error))
	{
		// The API answers 404 for a repository that has no release yet.
		return Fail(error, net_error.find("HTTP 404") != std::string::npos ? "No launcher release has been published yet"
		                   : net_error.empty()                              ? "Could not reach GitHub"
		                                                                    : net_error);
	}
	JsonValue root;
	if (!parse_json(body, root) || root.type != JsonValue::Type::Object)
	{
		return Fail(error, "GitHub sent an answer the launcher can't read");
	}
	out          = LauncherRelease {};
	out.tag      = Text(root, "tag_name");
	out.name     = Text(root, "name");
	out.notes    = Text(root, "body");
	out.page_url = Text(root, "html_url");
	if (out.tag.empty())
	{
		// {"message": "Not Found"} when the repository has no release yet.
		const std::string message = Text(root, "message");
		return Fail(error, message == "Not Found" ? "No launcher release has been published yet" : message.empty() ? "No release found" : message);
	}
	if (const JsonValue* assets = Field(root, "assets", JsonValue::Type::Array); assets != nullptr && std::string(kAssetSuffix).size() != 0)
	{
		for (const auto& a: assets->a)
		{
			if (a.type != JsonValue::Type::Object)
			{
				continue;
			}
			const std::string name = Text(a, "name");
			if (EndsWith(Lower(name), Lower(kAssetSuffix)))
			{
				out.asset_name = name;
				out.asset_url  = Text(a, "browser_download_url");
				if (const JsonValue* size = Field(a, "size", JsonValue::Type::Number))
				{
					out.asset_size = static_cast<uint64_t>(size->n);
				}
				break;
			}
		}
	}
	return true;
}

std::string current_executable()
{
#ifdef _WIN32
	std::wstring buf(MAX_PATH, L'\0');
	for (;;)
	{
		const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
		if (n == 0)
		{
			return {};
		}
		if (n < buf.size())
		{
			buf.resize(n);
			break;
		}
		buf.resize(buf.size() * 2);
	}
	return path_utf8(fs::path(buf));
#elif defined(__linux__)
	std::error_code ec;
	const fs::path  p = fs::read_symlink("/proc/self/exe", ec);
	return ec ? std::string() : path_utf8(p);
#else
	return {};
#endif
}

bool install_launcher_update(const LauncherRelease& release, const std::string& exe,
                             const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel,
                             std::string* new_exe, std::string* error)
{
	if (release.asset_url.empty())
	{
		return Fail(error, "Release " + release.tag + " has no download for this system");
	}
	const fs::path  exe_path = utf8_path(exe);
	const fs::path  dir      = exe_path.parent_path();
	const fs::path  work     = dir / kWorkDir;
	std::error_code ec;
	if (exe.empty() || dir.empty())
	{
		return Fail(error, "Could not find the launcher's own folder");
	}
	fs::remove_all(work, ec);
	if (!fs::create_directories(work / "payload", ec) || ec)
	{
		return Fail(error, "The launcher's folder (" + path_utf8(dir) + ") can't be written to. Download the release from GitHub and "
		                   "replace the launcher by hand.");
	}
	auto cleanup = [&] { std::error_code rec; fs::remove_all(work, rec); };

	// Download.
	const fs::path package = work / ("package" + std::string(kAssetSuffix));
	{
		std::ofstream out(package, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			cleanup();
			return Fail(error, "Could not write " + path_utf8(package));
		}
		uint64_t    done  = 0;
		uint64_t    total = release.asset_size;
		std::string net_error;
		const bool  ok = http_get(
		    release.asset_url,
		    [&](const char* p, size_t n)
		    {
			    out.write(p, static_cast<std::streamsize>(n));
			    done += n;
			    if (progress)
			    {
				    progress(done, total);
			    }
			    return out.good() && (cancel == nullptr || !cancel->load());
		    },
		    &total, &net_error);
		out.close();
		if (cancel != nullptr && cancel->load())
		{
			cleanup();
			return Fail(error, "Cancelled");
		}
		if (!ok || !out)
		{
			cleanup();
			return Fail(error, net_error.empty() ? "The download failed" : "The download failed: " + net_error);
		}
	}

	// Unpack and find the new launcher.
	std::string unpack_error;
	if (!unpack_release_archive(path_utf8(package), path_utf8(work / "payload"), &unpack_error))
	{
		cleanup();
		return Fail(error, unpack_error);
	}
	const std::string exe_name = path_utf8(exe_path.filename());
	const fs::path    root     = FindPayloadRoot(work / "payload", exe_name);
	if (root.empty())
	{
		cleanup();
		return Fail(error, "The release archive holds no " + exe_name);
	}
	if (cancel != nullptr && cancel->load())
	{
		cleanup();
		return Fail(error, "Cancelled");
	}

	// Swap the files in: each file that is replaced moves aside to <name>.old first. A failure puts back what was
	// moved, so the old launcher keeps working.
	struct Moved
	{
		fs::path target;
		bool     had_old = false;
	};
	std::vector<Moved> moved;
	auto rollback = [&]
	{
		for (auto it = moved.rbegin(); it != moved.rend(); ++it)
		{
			std::error_code rec;
			fs::remove(it->target, rec);
			if (it->had_old)
			{
				fs::rename(fs::path(it->target).concat(".old"), it->target, rec);
			}
		}
	};
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code ie;
		const fs::path  rel    = it->path().lexically_relative(root);
		const fs::path  target = dir / rel;
		if (it->is_directory(ie))
		{
			fs::create_directories(target, ie);
			continue;
		}
		Moved m;
		m.target = target;
		if (fs::exists(target, ie))
		{
			const fs::path old = fs::path(target).concat(".old");
			fs::remove(old, ie);
			ie.clear();
			fs::rename(target, old, ie);
			if (ie)
			{
				rollback();
				cleanup();
				return Fail(error, "Could not replace " + path_utf8(target) + ": " + ie.message());
			}
			m.had_old = true;
		}
		fs::rename(it->path(), target, ie);
		if (ie)
		{
			// Another drive: copy instead.
			ie.clear();
			fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ie);
		}
		moved.push_back(m);
		if (ie)
		{
			rollback();
			cleanup();
			return Fail(error, "Could not write " + path_utf8(target) + ": " + ie.message());
		}
	}
	if (ec)
	{
		rollback();
		cleanup();
		return Fail(error, "Could not read the unpacked release: " + ec.message());
	}
#ifndef _WIN32
	// tar keeps the mode, but make sure the launcher can run.
	fs::permissions(exe_path, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
#endif
	cleanup();
	if (new_exe != nullptr)
	{
		*new_exe = path_utf8(exe_path);
	}
	return true;
}

void cleanup_launcher_update(const std::string& exe_dir)
{
	std::error_code ec;
	const fs::path  dir = utf8_path(exe_dir);
	if (dir.empty())
	{
		return;
	}
	fs::remove_all(dir / kWorkDir, ec);
	// Only the files an update set aside: "<name>.old" next to a "<name>" that exists.
	for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code ie;
		const fs::path& p = it->path();
		if (p.extension() == ".old" && it->is_regular_file(ie) && fs::exists(fs::path(p).replace_extension(), ie))
		{
			fs::remove(p, ie);
		}
	}
}

bool start_detached(const std::string& exe)
{
#ifdef _WIN32
	std::wstring        path = utf8_path(exe).wstring();
	std::wstring        cmd  = L"\"" + path + L"\"";
	STARTUPINFOW        si {sizeof(si)};
	PROCESS_INFORMATION pi {};
	const std::wstring  cwd = utf8_path(exe).parent_path().wstring();
	if (CreateProcessW(path.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP, nullptr, cwd.c_str(), &si, &pi) == FALSE)
	{
		return false;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
#else
	const pid_t pid = fork();
	if (pid < 0)
	{
		return false;
	}
	if (pid == 0)
	{
		// A grandchild, so it is not this process's child (no zombie, no death signal).
		setsid();
		if (fork() != 0)
		{
			_exit(0);
		}
		execl(exe.c_str(), exe.c_str(), static_cast<char*>(nullptr));
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return true;
#endif
}

} // namespace kyty_launcher
