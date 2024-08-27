#ifndef BIRON_ALLOCATOR_INL
#define BIRON_ALLOCATOR_INL
#include <biron/util/types.inl>

namespace Biron {

struct Allocator {
	virtual void* allocate(Ulen size) noexcept = 0;
	virtual void deallocate(void* old, Ulen size) noexcept = 0;
};

template<Ulen E>
struct InlineAllocator : Allocator {
	static inline constexpr const Ulen ALIGN = 16;
	static inline constexpr const Ulen CAPACITY = ((E + ALIGN - 1) / ALIGN) * ALIGN;
	virtual void* allocate(Ulen size) noexcept override {
		const Ulen bytes = ((size + ALIGN - 1) / ALIGN) * ALIGN;
		if (m_offset + bytes >= CAPACITY) {
			return nullptr;
		}
		void *const result = &m_data[m_offset];
		m_offset += bytes;
		return result;
	}
	virtual void deallocate(void* old, Ulen size) noexcept override {
		if (old && &m_data[m_offset] == old) {
			m_offset -= size;
		}
	}
	Bool owns(const void* old, Ulen size) noexcept {
		if (!old) return false;
		const auto data = reinterpret_cast<const Uint8*>(old);
		return data >= m_data && data + size < &m_data[CAPACITY];
	}
private:
	alignas(16) Uint8 m_data[CAPACITY];
	Ulen m_offset = 0;
};

struct TemporaryAllocator : Allocator {
	static inline constexpr const Ulen ALIGN = 16;
	static inline constexpr const Ulen MIN_CHUNK_SIZE = 2 * 1024 * 1024; // 2 MiB
	constexpr TemporaryAllocator(Allocator& allocator) noexcept
		: m_tail{nullptr}
		, m_allocator{allocator}
	{
	}
	~TemporaryAllocator() noexcept {
		clear();
	}
	virtual void* allocate(Ulen size) noexcept override {
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
	virtual void deallocate(void* old, Ulen size) noexcept override {
		if (old && &m_tail->data[m_tail->offset] == old) {
			m_tail->offset -= size;
		}
	}
	void clear() {
		Chunk* chunk = m_tail;
		while (chunk) {
			Chunk* prev = chunk->prev;
			m_allocator.deallocate(chunk, sizeof(Chunk) + chunk->capacity);
			chunk = prev;
		}
	}
private:
	struct alignas(ALIGN) Chunk {
		Ulen   capacity;
		Ulen   offset;
		Chunk* next;
		Chunk* prev;
		Uint8  data[];
	};
	Chunk* m_tail;
	Allocator& m_allocator;
};

struct ScratchAllocator : Allocator {
	static inline constexpr const Ulen INSITU = 16384; // 16 KiB
	constexpr ScratchAllocator(Allocator& allocator) noexcept
		: m_temporary{allocator}
	{
	}
	virtual void* allocate(Ulen size) noexcept override {
		if (auto result = m_inline.allocate(size)) {
			return result;
		}
		return m_temporary.allocate(size);
	}
	virtual void deallocate(void* old, Ulen size) noexcept override {
		if (m_inline.owns(old, size)) {
			m_inline.deallocate(old, size);
		} else {
			m_temporary.deallocate(old, size);
		}
	}
private:
	InlineAllocator<INSITU> m_inline;
	TemporaryAllocator m_temporary;
};

} // namespace Biron

#endif // BIRON_ALLOCATOR_INL