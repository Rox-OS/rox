#include <stdlib.h> // malloc, free
#include <string.h> // memcpy
#include <stdio.h> // fopen, FILE, fclose, fseek, fread

#include <biron/util/system.inl>

namespace Biron {

static void* mem_allocate(const System&, Ulen bytes) noexcept {
	return malloc(bytes);
}

static void mem_deallocate(const System&, void* memory, Ulen) noexcept {
	free(memory);
}

static SysFile* file_open(const System& system,
                          StringView filename) noexcept
{
	SystemAllocator allocator{system};
	char* name = filename.terminated(allocator);
	if (!name) {
		return nullptr;
	}
	FILE* fp = fopen(name, "rb");
	allocator.deallocate(name, filename.length() + 1);
	if (!fp) {
		return nullptr;
	}
	return reinterpret_cast<SysFile*>(fp);
}

static void file_close(const System&, SysFile* file) noexcept {
	fclose(reinterpret_cast<FILE *>(file));
}

static Uint64 file_read(const System&,
                        SysFile* file,
                        Uint64 offset,
                        void* data,
                        Uint64 length)
{
	auto fp = reinterpret_cast<FILE *>(file);
	if (feof(fp) != 0) {
		return 0;
	}
	if (fseek(fp, offset, SEEK_SET) != 0) {
		return 0;
	}
	fread(data, length, 1, fp);
	return ftell(fp) - offset;
}

static Bool term_out(const System&, StringView content) noexcept {
	fwrite(content.data(), content.length(), 1, stdout);
	fflush(stdout);
	return ferror(stdout) == 0;
}

static Bool term_err(const System&, StringView content) noexcept {
	fwrite(content.data(), content.length(), 1, stderr);
	fflush(stderr);
	return ferror(stderr) == 0;
}

extern const System SYSTEM_STDC = {
	mem_allocate,
	mem_deallocate,
	file_open,
	file_close,
	file_read,
	term_out,
	term_err,
};

} // namespace Biron