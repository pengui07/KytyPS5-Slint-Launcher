#include "library.h"

#include "net.h"
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <winhttp.h>
#else
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#endif

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

constexpr uintmax_t kMaxParamJsonBytes = 4u * 1024u * 1024u;

std::string Trim(const std::string& s)
{
	size_t b = 0;
	size_t e = s.size();
	while (b < e && std::isspace(static_cast<unsigned char>(s[b])) != 0)
	{
		b++;
	}
	while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0)
	{
		e--;
	}
	return s.substr(b, e - b);
}

std::string Upper(std::string s)
{
	for (auto& c: s)
	{
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return s;
}

const JsonValue* Member(const JsonValue& v, const std::string& key)
{
	if (v.type != JsonValue::Type::Object)
	{
		return nullptr;
	}
	auto it = v.o.find(key);
	return it != v.o.end() ? &it->second : nullptr;
}

std::string StringMember(const JsonValue& v, const std::string& key)
{
	const JsonValue* m = Member(v, key);
	return m != nullptr && m->type == JsonValue::Type::String ? Trim(m->s) : std::string();
}

std::string LocalizedTitle(const JsonValue& root)
{
	const JsonValue* loc = Member(root, "localizedParameters");
	if (loc == nullptr || loc->type != JsonValue::Type::Object)
	{
		return {};
	}
	auto title_of = [&](const std::string& lang)
	{
		const JsonValue* l = Member(*loc, lang);
		return l != nullptr ? StringMember(*l, "titleName") : std::string();
	};
	std::string t = title_of(StringMember(*loc, "defaultLanguage"));
	if (t.empty())
	{
		t = title_of("en-US");
	}
	if (t.empty())
	{
		for (const auto& kv: loc->o)
		{
			if (kv.second.type == JsonValue::Type::Object)
			{
				t = StringMember(kv.second, "titleName");
				if (!t.empty())
				{
					break;
				}
			}
		}
	}
	return t;
}

// requiredSystemSoftwareVersion is "0x" + 6 decimal digits (MMmmpp) + 10 hex digits.
std::string FirmwareVersion(const std::string& encoded)
{
	if (encoded.size() != 18 || encoded[0] != '0' || (encoded[1] != 'x' && encoded[1] != 'X'))
	{
		return {};
	}
	for (size_t i = 2; i < 8; i++)
	{
		if (std::isdigit(static_cast<unsigned char>(encoded[i])) == 0)
		{
			return {};
		}
	}
	for (size_t i = 8; i < 18; i++)
	{
		if (std::isxdigit(static_cast<unsigned char>(encoded[i])) == 0)
		{
			return {};
		}
	}
	std::string v = std::to_string(std::stoi(encoded.substr(2, 2))) + "." + encoded.substr(4, 2);
	if (encoded.substr(6, 2) != "00")
	{
		v += "." + encoded.substr(6, 2);
	}
	return v;
}

// Size on disk, as Explorer shows it: compressed (and sparse) files count what they really
// take, rounded up to whole clusters.
uint64_t DirectorySize(const fs::path& dir)
{
	uint64_t total   = 0;
	uint64_t cluster = 0;
#ifdef _WIN32
	{
		wchar_t volume[MAX_PATH] = {};
		DWORD   sectors = 0, bytes = 0, free_clusters = 0, clusters = 0;
		if (GetVolumePathNameW(dir.wstring().c_str(), volume, MAX_PATH) != FALSE &&
		    GetDiskFreeSpaceW(volume, &sectors, &bytes, &free_clusters, &clusters) != FALSE)
		{
			cluster = static_cast<uint64_t>(sectors) * bytes;
		}
	}
#endif
	std::error_code ec;
	fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
	for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		std::error_code fec;
		if (!it->is_regular_file(fec) || fec)
		{
			continue;
		}
		uint64_t n = 0;
#ifdef _WIN32
		DWORD high = 0;
		SetLastError(NO_ERROR);
		DWORD low  = GetCompressedFileSizeW(it->path().wstring().c_str(), &high);
		if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
		{
			n = it->file_size(fec);
		} else
		{
			n = (static_cast<uint64_t>(high) << 32u) | low;
		}
#else
		{
			struct stat st {};
			if (stat(it->path().c_str(), &st) == 0)
			{
				n = static_cast<uint64_t>(st.st_blocks) * 512u; // allocated, like "size on disk"
			} else
			{
				n = it->file_size(fec);
			}
		}
#endif
		if (fec)
		{
			continue;
		}
		if (cluster != 0)
		{
			n = (n + cluster - 1) / cluster * cluster;
		}
		total += n;
	}
	return total;
}

GameStatus StatusFromText(const std::string& text)
{
	const std::string v = Trim(text);
	if (v == "InGame" || v == "In game")
	{
		return GameStatus::InGame;
	}
	if (v == "MainMenu" || v == "Main menu")
	{
		return GameStatus::MainMenu;
	}
	if (v == "Logo")
	{
		return GameStatus::Logo;
	}
	if (v == "DoesntBoot" || v == "Doesn't boot")
	{
		return GameStatus::DoesntBoot;
	}
	return GameStatus::Unknown;
}

std::vector<std::string> SplitRoots(const std::string& roots)
{
	std::vector<std::string> out;
	std::stringstream        ss(roots);
	std::string              part;
	while (std::getline(ss, part, ';'))
	{
		part = Trim(part);
		if (!part.empty())
		{
			out.push_back(part);
		}
	}
	return out;
}

} // namespace

std::string path_key(const std::string& path)
{
	std::string k = path_utf8(utf8_path(path).lexically_normal());
	for (auto& c: k)
	{
		c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	while (k.size() > 1 && k.back() == '/')
	{
		k.pop_back();
	}
	return k;
}

namespace {

// Case-insensitive form of a text column.
std::string LowerKey(std::string s)
{
	for (auto& c: s)
	{
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

int CompareText(const std::string& a, const std::string& b)
{
	const int c = LowerKey(a).compare(LowerKey(b));
	return c < 0 ? -1 : c > 0 ? 1 : 0;
}

} // namespace

GameSortKey game_sort_key_from_string(const std::string& key)
{
	if (key == "status")
	{
		return GameSortKey::Status;
	}
	if (key == "serial")
	{
		return GameSortKey::Serial;
	}
	if (key == "firmware")
	{
		return GameSortKey::Firmware;
	}
	if (key == "size")
	{
		return GameSortKey::Size;
	}
	if (key == "version")
	{
		return GameSortKey::Version;
	}
	if (key == "playtime")
	{
		return GameSortKey::Playtime;
	}
	if (key == "path")
	{
		return GameSortKey::Path;
	}
	if (key == "comment")
	{
		return GameSortKey::Comment;
	}
	return GameSortKey::Name;
}

const char* game_sort_key_name(GameSortKey key)
{
	switch (key)
	{
		case GameSortKey::Name: return "name";
		case GameSortKey::Status: return "status";
		case GameSortKey::Serial: return "serial";
		case GameSortKey::Firmware: return "firmware";
		case GameSortKey::Size: return "size";
		case GameSortKey::Version: return "version";
		case GameSortKey::Playtime: return "playtime";
		case GameSortKey::Path: return "path";
		case GameSortKey::Comment: return "comment";
	}
	return "name";
}

std::string favourite_key(const GameEntry& game)
{
	return path_key(game.title_id.empty() ? game.eboot : game.title_id);
}

void sort_games(std::vector<GameEntry>& games, GameSortKey key, bool descending, const std::set<std::string>& favourites)
{
	auto pinned  = [&favourites](const GameEntry& g) { return favourites.count(favourite_key(g)) != 0; };
	auto compare = [key](const GameEntry& a, const GameEntry& b)
	{
		switch (key)
		{
			case GameSortKey::Name: return CompareText(a.name, b.name);
			case GameSortKey::Status: return a.status < b.status ? -1 : a.status > b.status ? 1 : 0;
			case GameSortKey::Serial: return CompareText(a.title_id, b.title_id);
			case GameSortKey::Firmware: return CompareText(a.firmware_version, b.firmware_version);
			case GameSortKey::Size: return a.size < b.size ? -1 : a.size > b.size ? 1 : 0;
			case GameSortKey::Version: return CompareText(a.game_version, b.game_version);
			// Play time is a display string ("12h 30m") and empty until it is tracked: the name tie-break orders it.
			case GameSortKey::Playtime: return CompareText(a.play_time, b.play_time);
			case GameSortKey::Path: return CompareText(a.dir, b.dir);
			case GameSortKey::Comment: return CompareText(a.comment, b.comment);
		}
		return 0;
	};
	std::stable_sort(games.begin(), games.end(),
	                 [&](const GameEntry& a, const GameEntry& b)
	                 {
		                 // Pinned games stay on top in both directions.
		                 const bool pa = pinned(a);
		                 const bool pb = pinned(b);
		                 if (pa != pb)
		                 {
			                 return pa;
		                 }
		                 int c = compare(a, b);
		                 if (descending)
		                 {
			                 c = -c;
		                 }
		                 if (c != 0)
		                 {
			                 return c < 0;
		                 }
		                 // Ties always break ascending, so the order is deterministic.
		                 c = CompareText(a.name, b.name);
		                 if (c != 0)
		                 {
			                 return c < 0;
		                 }
		                 return a.title_id < b.title_id;
	                 });
}

std::string read_text_file(const std::string& path)
{
	std::ifstream f(utf8_path(path), std::ios::binary);
	if (!f)
	{
		return {};
	}
	std::stringstream ss;
	ss << f.rdbuf();
	std::string text = ss.str();
	if (text.rfind("\xEF\xBB\xBF", 0) == 0)
	{
		text.erase(0, 3);
	}
	return text;
}

bool write_text_file(const std::string& path, const std::string& text)
{
	std::error_code ec;
	fs::create_directories(utf8_path(path).parent_path(), ec);
	std::ofstream f(utf8_path(path), std::ios::binary | std::ios::trunc);
	return static_cast<bool>(f << text);
}

std::string read_title_id(const std::string& game)
{
	std::error_code ec;
	fs::path        dir = utf8_path(game);
	if (!fs::is_directory(dir, ec))
	{
		dir = dir.parent_path();
	}
	const fs::path param = dir / "sce_sys" / "param.json";
	if (!fs::is_regular_file(param, ec) || fs::file_size(param, ec) > kMaxParamJsonBytes)
	{
		return {};
	}
	JsonValue root;
	if (!parse_json(read_text_file(path_utf8(param)), root) || root.type != JsonValue::Type::Object)
	{
		return {};
	}
	std::string id = StringMember(root, "titleId");
	if (id.empty())
	{
		// Like the Qt launcher: characters 7..15 of contentId "XXXXXX-TITLEID_00-...".
		const std::string content_id = StringMember(root, "contentId");
		if (content_id.size() == 36 && content_id[6] == '-' && content_id[16] == '_')
		{
			id = content_id.substr(7, 9);
		}
	}
	for (char c: id)
	{
		if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '-')
		{
			return {};
		}
	}
	return id;
}

const char* status_text(GameStatus s)
{
	switch (s)
	{
		case GameStatus::InGame: return "In game";
		case GameStatus::MainMenu: return "Main menu";
		case GameStatus::Logo: return "Logo";
		case GameStatus::DoesntBoot: return "Doesn't boot";
		case GameStatus::Unknown: break;
	}
	return "Unknown";
}

std::string format_size(uint64_t bytes)
{
	char         buf[32];
	const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
	if (gib >= 1.0)
	{
		std::snprintf(buf, sizeof(buf), "%.2f GB", gib);
	} else
	{
		std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	}
	return buf;
}

std::vector<GameEntry> scan_games(const std::string& roots, SizeCache& sizes)
{
	std::vector<GameEntry> games;
	std::set<std::string>  seen;
	std::error_code        ec;

	auto add = [&](const fs::path& dir)
	{
		const std::string dir_utf8 = path_utf8(dir);
		if (!seen.insert(path_key(dir_utf8)).second)
		{
			return;
		}
		GameEntry g;
		g.dir   = dir_utf8;
		g.eboot = path_utf8(dir / "eboot.bin");
		g.name  = path_utf8(dir.filename());

		std::error_code pec;
		const fs::path  param = dir / "sce_sys" / "param.json";
		if (fs::is_regular_file(param, pec) && fs::file_size(param, pec) <= kMaxParamJsonBytes)
		{
			JsonValue root;
			if (parse_json(read_text_file(path_utf8(param)), root) && root.type == JsonValue::Type::Object)
			{
				std::string title = LocalizedTitle(root);
				if (!title.empty())
				{
					g.name = title;
				}
				g.title_id     = StringMember(root, "titleId");
				g.game_version = StringMember(root, "appVersion");
				if (g.game_version.empty())
				{
					g.game_version = StringMember(root, "contentVersion");
				}
				g.firmware_version = FirmwareVersion(StringMember(root, "requiredSystemSoftwareVersion"));
			}
		}
		for (const char* art: {"pic1.png", "pic0.png"})
		{
			const fs::path pic = dir / "sce_sys" / art;
			if (fs::is_regular_file(pic, pec))
			{
				g.background = path_utf8(pic);
				break;
			}
		}
		const fs::path icon = dir / "sce_sys" / "icon0.png";
		if (fs::is_regular_file(icon, pec))
		{
			g.icon = path_utf8(icon);
		}

		const std::string key = path_key(dir_utf8);
		auto              sit = sizes.find(key);
		if (sit == sizes.end())
		{
			sit = sizes.emplace(key, DirectorySize(dir)).first;
		}
		g.size = sit->second;
		games.push_back(std::move(g));
	};

	for (const auto& root_text: SplitRoots(roots))
	{
		const fs::path root = utf8_path(root_text);
		if (!fs::is_directory(root, ec))
		{
			continue;
		}
		if (fs::exists(root / "eboot.bin", ec))
		{
			add(root);
			continue;
		}
		std::deque<fs::path> pending{root};
		while (!pending.empty())
		{
			fs::path cur = pending.front();
			pending.pop_front();
			for (fs::directory_iterator it(cur, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
			     it.increment(ec))
			{
				std::error_code dec;
				if (!it->is_directory(dec) || it->is_symlink(dec))
				{
					continue;
				}
				if (fs::exists(it->path() / "eboot.bin", dec))
				{
					add(it->path());
				} else
				{
					pending.push_back(it->path());
				}
			}
		}
	}

	std::stable_sort(games.begin(), games.end(),
	                 [](const GameEntry& a, const GameEntry& b)
	                 {
		                 auto lower = [](std::string s)
		                 {
			                 for (auto& c: s)
			                 {
				                 c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			                 }
			                 return s;
		                 };
		                 return lower(a.name) < lower(b.name);
	                 });
	return games;
}

CompatDb parse_compat(const std::string& json)
{
	CompatDb  db;
	JsonValue root;
	if (!parse_json(json, root) || root.type != JsonValue::Type::Object)
	{
		return db;
	}
	for (const auto& [id, entry]: root.o)
	{
		if (entry.type != JsonValue::Type::Object || Trim(id).empty())
		{
			continue;
		}
		CompatEntry e;
		e.status  = StatusFromText(StringMember(entry, "status"));
		const JsonValue* c = Member(entry, "comment");
		if (c != nullptr && c->type == JsonValue::Type::String)
		{
			e.comment = c->s;
		}
		db[Upper(Trim(id))] = std::move(e);
	}
	return db;
}

void apply_compat(std::vector<GameEntry>& games, const CompatDb& db)
{
	for (auto& g: games)
	{
		auto it = db.find(Upper(Trim(g.title_id)));
		if (it != db.end())
		{
			g.status  = it->second.status;
			g.comment = it->second.comment;
		}
	}
}

bool load_image_rgba(const std::string& path, int max_w, int max_h, std::vector<uint8_t>& rgba, int& w, int& h)
{
#ifdef _WIN32
	const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool          ok = false;
	{
		IWICImagingFactory*    factory = nullptr;
		IWICBitmapDecoder*     decoder = nullptr;
		IWICBitmapFrameDecode* frame   = nullptr;
		IWICBitmapScaler*      scaler  = nullptr;
		IWICFormatConverter*   conv    = nullptr;
		if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		    SUCCEEDED(factory->CreateDecoderFromFilename(utf8_path(path).wstring().c_str(), nullptr, GENERIC_READ,
		                                                 WICDecodeMetadataCacheOnDemand, &decoder)) &&
		    SUCCEEDED(decoder->GetFrame(0, &frame)))
		{
			UINT sw = 0;
			UINT sh = 0;
			frame->GetSize(&sw, &sh);
			double scale = std::min({1.0, static_cast<double>(max_w) / sw, static_cast<double>(max_h) / sh});
			UINT   dw    = std::max<UINT>(1, static_cast<UINT>(sw * scale));
			UINT   dh    = std::max<UINT>(1, static_cast<UINT>(sh * scale));
			if (sw > 0 && sh > 0 && SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
			    SUCCEEDED(scaler->Initialize(frame, dw, dh, WICBitmapInterpolationModeFant)) &&
			    SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
			    SUCCEEDED(conv->Initialize(scaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
			                                WICBitmapPaletteTypeCustom)))
			{
				rgba.resize(static_cast<size_t>(dw) * dh * 4);
				ok = SUCCEEDED(conv->CopyPixels(nullptr, dw * 4, static_cast<UINT>(rgba.size()), rgba.data()));
				w  = static_cast<int>(dw);
				h  = static_cast<int>(dh);
			}
		}
		for (IUnknown* o: std::initializer_list<IUnknown*>{conv, scaler, frame, decoder, factory})
		{
			if (o != nullptr)
			{
				o->Release();
			}
		}
	}
	if (SUCCEEDED(co))
	{
		CoUninitialize();
	}
	return ok;
#else
	int       iw = 0;
	int       ih = 0;
	int       channels = 0;
	stbi_uc*  pixels   = stbi_load(path.c_str(), &iw, &ih, &channels, 4);
	if (pixels == nullptr || iw <= 0 || ih <= 0)
	{
		return false;
	}
	// Scaled down to fit (never up), nearest neighbour: this is a blurred backdrop.
	const double scale = std::min({1.0, static_cast<double>(max_w) / iw, static_cast<double>(max_h) / ih});
	w                  = std::max(1, static_cast<int>(iw * scale));
	h                  = std::max(1, static_cast<int>(ih * scale));
	rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
	for (int y = 0; y < h; y++)
	{
		const int sy = std::min(ih - 1, static_cast<int>(y / scale));
		for (int x = 0; x < w; x++)
		{
			const int sx = std::min(iw - 1, static_cast<int>(x / scale));
			std::copy_n(pixels + (static_cast<size_t>(sy) * iw + sx) * 4, 4, rgba.data() + (static_cast<size_t>(y) * w + x) * 4);
		}
	}
	stbi_image_free(pixels);
	return true;
#endif
}

std::string fetch_compat_json()
{
#ifdef _WIN32
	std::string  body;
	HINTERNET    session = WinHttpOpen(L"Kyty-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
	                                   WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET    conn    = nullptr;
	HINTERNET    req     = nullptr;
	if (session != nullptr)
	{
		WinHttpSetTimeouts(session, 8000, 8000, 8000, 15000);
		conn = WinHttpConnect(session, L"kytyps5.github.io", INTERNET_DEFAULT_HTTPS_PORT, 0);
	}
	if (conn != nullptr)
	{
		req = WinHttpOpenRequest(conn, L"GET", L"/data/compatibility.json", nullptr, WINHTTP_NO_REFERER,
		                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	}
	bool ok = req != nullptr && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE &&
	          WinHttpReceiveResponse(req, nullptr) != FALSE;
	if (ok)
	{
		DWORD status = 0;
		DWORD len    = sizeof(status);
		ok           = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
		                                   &status, &len, WINHTTP_NO_HEADER_INDEX) != FALSE &&
		     status == 200;
	}
	while (ok)
	{
		DWORD avail = 0;
		if (WinHttpQueryDataAvailable(req, &avail) == FALSE || avail == 0)
		{
			break;
		}
		std::string chunk(avail, '\0');
		DWORD       got = 0;
		if (WinHttpReadData(req, chunk.data(), avail, &got) == FALSE || got == 0)
		{
			break;
		}
		body.append(chunk, 0, got);
	}
	if (req != nullptr)
	{
		WinHttpCloseHandle(req);
	}
	if (conn != nullptr)
	{
		WinHttpCloseHandle(conn);
	}
	if (session != nullptr)
	{
		WinHttpCloseHandle(session);
	}
	return ok ? body : std::string();
#else
	std::string body;
	std::string error;
	if (!http_get("https://kytyps5.github.io/data/compatibility.json", [&](const char* p, size_t n) { body.append(p, n); return true; }, nullptr,
	              &error))
	{
		return {};
	}
	return body;
#endif
}

} // namespace kyty_launcher
