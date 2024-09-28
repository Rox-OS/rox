#ifndef BIRON_SYSTEM_H
#define BIRON_SYSTEM_H
#include <biron/util/string.h>

namespace Biron {

struct SysFile;
struct SysThread;
struct SysMutex;
struct SysCond;

using MemAllocate     = void* (*)(const System&, Ulen);
using MemDeallocate   = void (*)(const System&, void*, Ulen);

using FileOpenFn      = SysFile* (*)(const System&, StringView);
using FileCloseFn     = void (*)(const System&, SysFile*);
using FileReadFn      = Uint64 (*)(const System&, SysFile*, Uint64, void*, Uint64);

using TermOutFn       = Bool (*)(const System&, StringView);
using TermErrFn       = Bool (*)(const System&, StringView);

using LibOpenFn       = void* (*)(const System&, StringView);
using LibCloseFn      = void (*)(const System&, void*);
using LibSymbolFn     = void* (*)(const System&, void*, StringView);

using ThreadCreateFn  = SysThread* (*)(const System&, void (*)(void*), void*);
using ThreadJoinFn    = void (*)(const System&, SysThread*);

using MutexCreateFn   = SysMutex* (*)(const System&);
using MutexDestroyFn  = void (*)(const System&, SysMutex*);
using MutexLockFn     = void (*)(const System&, SysMutex*);
using MutexUnlockFn   = void (*)(const System&, SysMutex*);

using CondCreateFn    = SysCond* (*)(const System&);
using CondDestroyFn   = void (*)(const System&, SysCond*);
using CondWaitFn      = void (*)(const System&, SysCond*, SysMutex*);
using CondSignalFn    = void (*)(const System&, SysCond*);
using CondBroadcastFn = void (*)(const System&, SysCond*);

struct System {
	MemAllocate     mem_allocate;
	MemDeallocate   mem_deallocate;
	FileOpenFn      file_open;
	FileCloseFn     file_close;
	FileReadFn      file_read;
	TermOutFn       term_out;
	TermErrFn       term_err;
	LibOpenFn       lib_open;
	LibCloseFn      lib_close;
	LibSymbolFn     lib_symbol;
	ThreadCreateFn  thread_create;
	ThreadJoinFn    thread_join;
	MutexCreateFn   mutex_create;
	MutexDestroyFn  mutex_destroy;
	MutexLockFn     mutex_lock;
	MutexUnlockFn   mutex_unlock;
	CondCreateFn    cond_create;
	CondDestroyFn   cond_destroy;
	CondWaitFn      cond_wait;
	CondSignalFn    cond_signal;
	CondBroadcastFn cond_broadcast;
};

} // namespace Biron

#endif // BIRON_SYSTEM_H