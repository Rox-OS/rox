#include <stdlib.h> // malloc, free
#include <string.h> // memcpy
#include <stdio.h> // fopen, FILE, fclose, fseek, fread

#include <dlfcn.h> // dlopen, dlclose, dlsym, RTLD_NOW

#include <pthread.h> // pthread_t, pthread_mutex_t, pthread_cond_t

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
	// Append ".so" to filename
	StringBuilder builder{scratch};
	builder.append(filename);
	builder.append('.');
	builder.append("so");
	builder.append('\0');
	if (!builder.valid()) {
		return nullptr;
	}
	return dlopen(builder.data(), RTLD_NOW);
}

static void lib_close(const System&, void* lib) noexcept {
	dlclose(lib);
}

static void* lib_symbol(const System& system, void* lib, StringView name) noexcept {
	SystemAllocator allocator{system};
	ScratchAllocator scratch{allocator};
	auto sym = name.terminated(scratch);
	if (!sym) {
		return nullptr;
	}
	auto addr = dlsym(lib, sym);
	scratch.deallocate(sym, name.length() + 1);
	return addr;
}

struct ThreadClosure {
	pthread_t handle;
	void (*entry)(void*);
	void* arg;
};

static void* thread_wrap(void *data) {
	auto thread = reinterpret_cast<ThreadClosure*>(data);
	thread->entry(thread->arg);
	return nullptr;
}

static SysThread* thread_create(const System& system, void (*fn)(void*), void* arg) noexcept {
	SystemAllocator allocator{system};
	auto thread = allocator.allocate_object<ThreadClosure>();
	if (!thread) {
		return nullptr;
	}
	thread->entry = fn;
	thread->arg = arg;
	if (pthread_create(&thread->handle, nullptr, thread_wrap, thread) != 0) {
		allocator.deallocate_object(thread);
		return nullptr;
	}
	return reinterpret_cast<SysThread*>(thread);
}

static void thread_join(const System& system, SysThread* opaque) noexcept {
	SystemAllocator allocator{system};
	auto thread = reinterpret_cast<ThreadClosure*>(opaque);
	void* ignore = nullptr;
	pthread_join(thread->handle, &ignore);
	allocator.deallocate_object(thread);
}

SysMutex* mutex_create(const System& system) noexcept {
	SystemAllocator allocator{system};
	auto mutex = allocator.allocate_object<pthread_mutex_t>();
	if (!mutex) {
		return nullptr;
	}
	if (pthread_mutex_init(mutex, nullptr) != 0) {
		allocator.deallocate_object(mutex);
		return nullptr;
	}
	return reinterpret_cast<SysMutex*>(mutex);
}

static void mutex_destroy(const System& system, SysMutex* opaque) noexcept {
	SystemAllocator allocator{system};
	auto mutex = reinterpret_cast<pthread_mutex_t*>(opaque);
	pthread_mutex_destroy(mutex);
	allocator.deallocate_object(mutex);
}

static void mutex_lock(const System&, SysMutex* opaque) noexcept {
	auto mutex = reinterpret_cast<pthread_mutex_t*>(opaque);
	pthread_mutex_lock(mutex);
}

static void mutex_unlock(const System&, SysMutex* opaque) noexcept {
	auto mutex = reinterpret_cast<pthread_mutex_t*>(opaque);
	pthread_mutex_unlock(mutex);
}

static SysCond* cond_create(const System& system) noexcept {
	SystemAllocator allocator{system};
	auto cond = allocator.allocate_object<pthread_cond_t>();
	if (!cond) {
		return nullptr;
	}
	if (pthread_cond_init(cond, nullptr) != 0) {
		allocator.deallocate_object(cond);
		return nullptr;
	}
	return reinterpret_cast<SysCond*>(cond);
}

static void cond_destroy(const System& system, SysCond* opaque) noexcept {
	SystemAllocator allocator{system};
	auto cond = reinterpret_cast<pthread_cond_t*>(opaque);
	pthread_cond_destroy(cond);
	allocator.deallocate_object(cond);
}

static void cond_wait(const System&, SysCond* opaque_cond, SysMutex* opaque_mutex) noexcept {
	auto cond = reinterpret_cast<pthread_cond_t*>(opaque_cond);
	auto mutex = reinterpret_cast<pthread_mutex_t*>(opaque_mutex);
	pthread_cond_wait(cond, mutex);
}

static void cond_signal(const System&, SysCond* opaque) noexcept {
	auto cond = reinterpret_cast<pthread_cond_t*>(opaque);
	pthread_cond_signal(cond);
}

static void cond_broadcast(const System&, SysCond* opaque) noexcept {
	auto cond = reinterpret_cast<pthread_cond_t*>(opaque);
	pthread_cond_broadcast(cond);
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
	thread_create,
	thread_join,
	mutex_create,
	mutex_destroy,
	mutex_lock,
	mutex_unlock,
	cond_create,
	cond_destroy,
	cond_wait,
	cond_signal,
	cond_broadcast,
};

} // namespace Biron