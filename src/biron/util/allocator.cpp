#include <biron/util/allocator.h>
#include <biron/util/system.inl>
#include <biron/util/numeric.inl>

namespace Biron {

template<typename T>
T atomic_fetch_max_explicit(Atomic<T>* pv, T v) noexcept {
	auto t = pv->load();
	while (!pv->compare_exchange_weak(t, max(v, t)))
		;
	return t;
}

void* SystemAllocator::allocate(Ulen size) noexcept {
	m_current_bytes += size;
	atomic_fetch_max_explicit(&m_peak_bytes, m_current_bytes.load());
	return m_system.mem_allocate(m_system, size);
}

void SystemAllocator::deallocate(void* old, Ulen size) noexcept {
	m_current_bytes -= size;
	m_system.mem_deallocate(m_system, old, size);
}

void* TemporaryAllocator::allocate(Ulen size) noexcept {
	const Ulen bytes = ((size + ALIGN - 1) / ALIGN) * ALIGN;
	if (m_tail && m_tail->offset + bytes < m_tail->capacity) {
		void *const result = &m_tail->data[m_tail->offset];
		m_tail->offset += bytes;
		return result;
	}
	const auto capacity = bytes > MIN_CHUNK_SIZE ? bytes : MIN_CHUNK_SIZE;
	const auto chunk = static_cast<Chunk*>(m_allocator.allocate(sizeof(Chunk) + capacity));
	if (!chunk) {
		return nullptr;
	}
	chunk->capacity = capacity;
	chunk->offset = 0;
	chunk->prev = m_tail;
	chunk->next = nullptr;
	m_tail = chunk;
	return allocate(size);
}

void TemporaryAllocator::deallocate(void* old, Ulen size) noexcept {
	if (old && &m_tail->data[m_tail->offset] == old) {
		m_tail->offset -= size;
	}
}

void TemporaryAllocator::clear() noexcept {
	Chunk* chunk = m_tail;
	while (chunk) {
		Chunk* prev = chunk->prev;
		m_allocator.deallocate(chunk, sizeof(Chunk) + chunk->capacity);
		chunk = prev;
	}
	m_tail = nullptr;
}

void* ScratchAllocator::allocate(Ulen size) noexcept {
	if (auto result = m_inline.allocate(size)) {
		return result;
	}
	return m_temporary.allocate(size);
}

void ScratchAllocator::deallocate(void* old, Ulen size) noexcept {
	if (m_inline.owns(old, size)) {
		m_inline.deallocate(old, size);
	} else {
		m_temporary.deallocate(old, size);
	}
}

void ScratchAllocator::clear() noexcept {
	m_inline.clear();
	m_temporary.clear();
}

} // namespace Biron