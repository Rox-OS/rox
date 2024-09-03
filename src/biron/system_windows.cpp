#if defined(_WIN32)
#include <stdlib.h> // malloc, free
#include <string.h> // memcpy
#include <stdio.h> // fopen, FILE, fclose, fseek, fread

#include <biron/util/system.inl>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#undef DELETE
#undef ERROR

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
	ScratchAllocator scratch{allocator};
	char* name = filename.terminated(scratch);
	if (!name) {
			return nullptr;
	}
	FILE* fp = fopen(name, "rb");
	scratch.deallocate(name, filename.length() + 1);
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

static void* lib_open(const System& system, StringView filename) noexcept {
	SystemAllocator allocator{system};
	ScratchAllocator scratch{allocator};
	// Append ".dll" to filename
	StringBuilder builder{scratch};
	builder.append(filename);
	builder.append('.');
	builder.append("dll");
	builder.append('\0');
	if (!builder.valid()) {
			return nullptr;
	}
	return LoadLibraryA(builder.data());
}

static void lib_close(const System&, void* lib) noexcept {
	FreeLibrary(reinterpret_cast<HMODULE>(lib));
}

static void* lib_symbol(const System& system, void* lib, StringView name) noexcept {
	SystemAllocator allocator{system};
	ScratchAllocator scratch{allocator};
	auto sym = name.terminated(scratch);
	if (!sym) {
			return nullptr;
	}
	auto addr = GetProcAddress(reinterpret_cast<HMODULE>(lib), sym);
	scratch.deallocate(sym, name.length() + 1);
	return addr;
}

extern const System SYSTEM = {
	mem_allocate,
	mem_deallocate,
	file_open,
	file_close,
	file_read,
	term_out,
	term_err,
	lib_open,
	lib_close,
	lib_symbol,
};

} // namespace Biron
#endif