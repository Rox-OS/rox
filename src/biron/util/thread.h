#ifndef BIRON_THREAD_H
#define BIRON_THREAD_H
#include <biron/util/maybe.inl>

namespace Biron {

struct SysThread;
struct SysMutex;
struct SysCond;

struct System;

struct Thread {
	static Maybe<Thread> make(const System& system, void (*fn)(void*), void* data) noexcept;
	constexpr Thread(Thread&& other) noexcept
		: m_system{other.m_system}
		, m_thread{exchange(other.m_thread, nullptr)}
	{
	}
	~Thread() noexcept;
private:
	constexpr Thread(const System& system, SysThread* thread) noexcept
		: m_system{system}
		, m_thread{thread}
	{
	}
	const System& m_system;
	SysThread*    m_thread;
};

struct Mutex {
	static Maybe<Mutex> make(const System& system) noexcept;
	constexpr Mutex(Mutex&& other) noexcept
		: m_system{other.m_system}
		, m_mutex{exchange(other.m_mutex, nullptr)}
	{
	}
	~Mutex() noexcept;
	void lock() noexcept;
	void unlock() noexcept;
private:
	friend struct Cond;
	constexpr Mutex(const System& system, SysMutex* mutex) noexcept
		: m_system{system}
		, m_mutex{mutex}
	{
	}
	const System& m_system;
	SysMutex*     m_mutex;
};

struct Cond {
	static Maybe<Cond> make(const System& system) noexcept;
	constexpr Cond(Cond&& other) noexcept
		: m_system{other.m_system}
		, m_cond{exchange(other.m_cond, nullptr)}
	{
	}
	~Cond() noexcept;
	void wait(Mutex& mutex) noexcept;
	void signal() noexcept;
	void broadcast() noexcept;
private:
	constexpr Cond(const System& system, SysCond* cond) noexcept
		: m_system{system}
		, m_cond{cond}
	{
	}
	const System& m_system;
	SysCond*      m_cond;
};

} // namespace Biron

#endif // BIRON_THREAD_H