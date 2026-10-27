#include "zar.h"

#include "settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include <zarchive/zarchivereader.h>
#include <zarchive/zarchivewriter.h>

namespace kyty_launcher {

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = "!/";

// Members are looked up with forward slashes and no leading slash.
std::string MemberKey(std::string member)
{
	std::replace(member.begin(), member.end(), '\\', '/');
	while (!member.empty() && member.front() == '/')
	{
		member.erase(member.begin());
	}
	return member;
}

} // namespace

bool is_zar_path(const std::string& path)
{
	if (path.size() < 4)
	{
		return false;
	}
	std::string ext = path.substr(path.size() - 4);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext == ".zar";
}

std::string zar_member_path(const std::string& archive, const std::string& member)
{
	return archive + kMarker + MemberKey(member);
}

bool split_zar_member_path(const std::string& path, std::string* archive, std::string* member)
{
	// The first "!/" after a ".zar": a folder name may hold a '!' of its own.
	for (size_t pos = path.find(kMarker); pos != std::string::npos; pos = path.find(kMarker, pos + 1))
	{
		if (is_zar_path(path.substr(0, pos)))
		{
			if (archive != nullptr)
			{
				*archive = path.substr(0, pos);
			}
			if (member != nullptr)
			{
				*member = path.substr(pos + 2);
			}
			return true;
		}
	}
	return false;
}

struct ZarReader::Impl
{
	std::unique_ptr<ZArchiveReader> reader;
};

ZarReader::ZarReader(std::unique_ptr<Impl> impl): m_impl(std::move(impl)) {}

ZarReader::~ZarReader() = default;

std::shared_ptr<ZarReader> ZarReader::Open(const std::string& archive)
{
	std::error_code ec;
	if (!fs::is_regular_file(utf8_path(archive), ec))
	{
		return nullptr;
	}
	auto impl    = std::make_unique<Impl>();
	impl->reader = std::unique_ptr<ZArchiveReader>(ZArchiveReader::OpenFromFile(utf8_path(archive)));
	if (!impl->reader)
	{
		return nullptr;
	}
	return std::shared_ptr<ZarReader>(new ZarReader(std::move(impl)));
}

bool ZarReader::FileExists(const std::string& member)
{
	return m_impl->reader->LookUp(MemberKey(member), true, false) != ZARCHIVE_INVALID_NODE;
}

bool ZarReader::ReadFile(const std::string& member, uint64_t max_size, std::string& out)
{
	const ZArchiveNodeHandle node = m_impl->reader->LookUp(MemberKey(member), true, false);
	if (node == ZARCHIVE_INVALID_NODE)
	{
		return false;
	}
	const uint64_t size = m_impl->reader->GetFileSize(node);
	if (size > max_size)
	{
		return false;
	}
	out.resize(static_cast<size_t>(size));
	return m_impl->reader->ReadFromFile(node, 0, size, out.data()) == size;
}

std::vector<ZarReader::Entry> ZarReader::List(const std::string& member)
{
	std::vector<Entry>       entries;
	const ZArchiveNodeHandle node = m_impl->reader->LookUp(MemberKey(member), false, true);
	if (node == ZARCHIVE_INVALID_NODE)
	{
		return entries;
	}
	const uint32_t count = m_impl->reader->GetDirEntryCount(node);
	for (uint32_t i = 0; i < count && i < (1u << 20u); i++)
	{
		ZArchiveReader::DirEntry e {};
		if (m_impl->reader->GetDirEntry(node, i, e))
		{
			entries.push_back({std::string(e.name), e.isFile});
		}
	}
	return entries;
}

bool read_game_file(const std::string& path, uint64_t max_size, std::string& out)
{
	std::string archive;
	std::string member;
	if (split_zar_member_path(path, &archive, &member))
	{
		const auto reader = ZarReader::Open(archive);
		return reader && reader->ReadFile(member, max_size, out);
	}
	std::error_code ec;
	const fs::path  p    = utf8_path(path);
	const uintmax_t size = fs::file_size(p, ec);
	if (ec || size > max_size)
	{
		return false;
	}
	std::ifstream f(p, std::ios::binary);
	if (!f)
	{
		return false;
	}
	out.resize(static_cast<size_t>(size));
	f.read(out.data(), static_cast<std::streamsize>(size));
	return static_cast<uintmax_t>(f.gcount()) == size;
}

bool pack_zar(const std::string& directory, const std::string& archive, const std::function<void(uint64_t, uint64_t)>& progress,
              const std::atomic<bool>* cancel, std::string* error)
{
	auto fail = [&](const std::string& why)
	{
		if (error != nullptr)
		{
			*error = why;
		}
		return false;
	};
	const fs::path  dir = utf8_path(directory);
	const fs::path  out = utf8_path(archive);
	std::error_code ec;
	if (!fs::is_directory(dir, ec))
	{
		return fail("Not a folder: " + directory);
	}
	if (fs::exists(out, ec))
	{
		return fail(archive + " already exists");
	}

	struct Item
	{
		fs::path path;
		uint64_t size    = 0;
		bool     is_file = false;
	};
	std::vector<Item> items;
	uint64_t          total = 0;
	// Folder links are followed, as the emulator does when it runs the folder.
	fs::recursive_directory_iterator it(dir, fs::directory_options::follow_directory_symlink, ec);
	for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		std::error_code ie;
		const bool      is_dir = it->is_directory(ie);
		if (ie)
		{
			return fail("Could not read " + path_utf8(it->path()) + ": " + ie.message());
		}
		Item item;
		item.path    = it->path();
		item.is_file = !is_dir;
		if (item.is_file)
		{
			item.size = static_cast<uint64_t>(it->file_size(ie));
			if (ie)
			{
				return fail("Could not read " + path_utf8(it->path()) + ": " + ie.message());
			}
		}
		total += item.size;
		items.push_back(std::move(item));
	}
	if (ec)
	{
		return fail("Could not list " + directory + ": " + ec.message());
	}

	struct Output
	{
		std::ofstream file;
		bool          ok = true;
	} output;
	output.file.open(out, std::ios::binary | std::ios::trunc);
	if (!output.file)
	{
		return fail("Could not create " + archive);
	}
	bool        ok = true;
	std::string why;
	{
		ZArchiveWriter writer(
		    [](int32_t, void*) {},
		    [](const void* data, size_t size, void* ctx)
		    {
			    auto& o = *static_cast<Output*>(ctx);
			    o.file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
			    o.ok = o.ok && o.file.good();
		    },
		    &output);
		std::vector<char> buffer(1u << 20u);
		uint64_t          done = 0;
		for (size_t i = 0; ok && i < items.size(); i++)
		{
			const Item&       item = items[i];
			// Archive names use forward slashes on every system.
			const auto        rel = item.path.lexically_relative(dir).generic_u8string();
			const std::string name(rel.begin(), rel.end());
			if (!item.is_file)
			{
				if (!writer.MakeDir(name.c_str(), true))
				{
					ok  = false;
					why = "Could not add the folder " + name;
				}
				continue;
			}
			std::ifstream in(item.path, std::ios::binary);
			if (!in || !writer.StartNewFile(name.c_str()))
			{
				ok  = false;
				why = "Could not read " + path_utf8(item.path);
				break;
			}
			for (uint64_t left = item.size; left != 0;)
			{
				const auto chunk = static_cast<std::streamsize>(std::min<uint64_t>(left, buffer.size()));
				in.read(buffer.data(), chunk);
				const auto got = in.gcount();
				if (got <= 0)
				{
					ok  = false;
					why = "Could not read " + path_utf8(item.path);
					break;
				}
				writer.AppendData(buffer.data(), static_cast<size_t>(got));
				done += static_cast<uint64_t>(got);
				left -= static_cast<uint64_t>(got);
				if (progress)
				{
					progress(done, total);
				}
				if (!output.ok)
				{
					ok  = false;
					why = "Could not write " + archive + " (is the disk full?)";
					break;
				}
				if (cancel != nullptr && cancel->load())
				{
					ok  = false;
					why = "Cancelled";
					break;
				}
			}
		}
		if (ok)
		{
			writer.Finalize();
		}
	}
	output.file.flush();
	ok = ok && output.ok && output.file.good();
	output.file.close();
	if (!ok)
	{
		fs::remove(out, ec);
		return fail(why.empty() ? "Could not write " + archive : why);
	}
	return true;
}

} // namespace kyty_launcher
