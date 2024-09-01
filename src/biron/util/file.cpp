#include <biron/util/file.h>
#include <biron/util/system.inl>

namespace Biron {

File::File(File&& other) noexcept
	: m_system{other.m_system}
	, m_file{exchange(other.m_file, nullptr)}
{
}

File::~File() noexcept {
	if (m_file) {
		m_system.file_close(m_system, m_file);
	}
}

Maybe<File> File::open(const System& system, StringView name) noexcept {
	auto file = system.file_open(system, name);
	if (!file) {
		return None{};
	}
	return File { system, file };
}

Uint64 File::read(Uint64 offset, void *data, Uint64 length) const noexcept {
	return m_system.file_read(m_system, m_file, offset, data, length);
}

} // namespace Biron