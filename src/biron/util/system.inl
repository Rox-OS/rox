#ifndef BIRON_SYSTEM_H
#define BIRON_SYSTEM_H
#include <biron/util/string.h>

namespace Biron {

struct SysFile;

using MemAllocate   = void* (*)(const System&, Ulen);
using MemDeallocate = void (*)(const System&, void*, Ulen);

using FileOpenFn    = SysFile* (*)(const System&, StringView);
using FileCloseFn   = void (*)(const System&, SysFile*);
using FileReadFn    = Uint64 (*)(const System&, SysFile*, Uint64, void*, Uint64);

using TermOutFn     = Bool (*)(const System&, StringView);
using TermErrFn     = Bool (*)(const System&, StringView);

using LibOpenFn     = void* (*)(const System&, StringView);
using LibCloseFn    = void (*)(const System&, void*);
using LibSymbolFn   = void* (*)(const System&, void*, StringView);

struct System {
	MemAllocate   mem_allocate;
	MemDeallocate mem_deallocate;
	FileOpenFn    file_open;
	FileCloseFn   file_close;
	FileReadFn    file_read;
	TermOutFn     term_out;
	TermErrFn     term_err;
	LibOpenFn     lib_open;
	LibCloseFn    lib_close;
	LibSymbolFn   lib_symbol;
};

} // namespace Biron

#endif // BIRON_SYSTEM_H