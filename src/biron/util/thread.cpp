#include <biron/util/thread.h>
#include <biron/util/system.inl>

namespace Biron {

Maybe<Thread> Thread::make(const System& system, void (*fn)(void*), void* data) noexcept {
	auto thread = system.thread_create(system, fn, data);
	if (!thread) {
		return None{};
	}
	return Thread { system, thread };
}

Thread::~Thread() noexcept {
	if (m_thread) {
		m_system.thread_join(m_system, m_thread);
	}
}

Maybe<Mutex> Mutex::make(const System& system) noexcept {
	auto mutex = system.mutex_create(system);
	if (!mutex) {
		return None{};
	}
	return Mutex { system, mutex };
}

Mutex::~Mutex() noexcept {
	if (m_mutex) {
		m_system.mutex_destroy(m_system, m_mutex);
	}
}

void Mutex::lock() noexcept {
	m_system.mutex_lock(m_system, m_mutex);
}

void Mutex::unlock() noexcept {
	m_system.mutex_unlock(m_system, m_mutex);
}

Maybe<Cond> Cond::make(const System& system) noexcept {
	auto cond = system.cond_create(system);
	if (!cond) {
		return None{};
	}
	return Cond { system, cond };
}

void Cond::wait(Mutex& mutex) noexcept {
	m_system.cond_wait(m_system, m_cond, mutex.m_mutex);
}

void Cond::signal() noexcept {
	m_system.cond_signal(m_system, m_cond);
}

void Cond::broadcast() noexcept {
	m_system.cond_broadcast(m_system, m_cond);
}

} // namespace Biron