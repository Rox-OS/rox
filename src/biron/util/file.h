#ifndef BIRON_FILE_H
#define BIRON_FILE_H
#include <biron/util/maybe.inl>
#include <biron/util/string.h>

namespace Biron {

struct System;
struct SysFile;

struct File {
	File(File&& other) noexcept;
	~File() noexcept;
	static Maybe<File> open(const System& system, StringView name) noexcept;
	Uint64 read(Uint64 offset, void *data, Uint64 length) const noexcept;
private:
	constexpr File(const System& system, SysFile* file) noexcept
		: m_system{system}
		, m_file{file}
	{
	}
	const System& m_system;
	SysFile* m_file;
};

} // namespace Biron

#endif // BIRON_FILE_H