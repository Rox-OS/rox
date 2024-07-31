#ifndef BIRON_ARRAY_INL
#define BIRON_ARRAY_INL
#include <biron/util/allocator.inl>
#include <biron/util/numeric.inl>
#include <biron/util/maybe.inl>

namespace Biron {

template<typename T>
struct Array;

template<typename T>
struct Array {
	constexpr Array(Allocator& allocator)
		: m_data{nullptr}
		, m_length{0}
		, m_capacity{0}
		, m_allocator{allocator}
	{
	}
	constexpr Array(Array&& other) noexcept
		: m_data{exchange(other.m_data, nullptr)}
		, m_length{exchange(other.m_length, 0)}
		, m_capacity{exchange(other.m_capacity, 0)}
		, m_allocator{other.m_allocator}
	{
	}
	~Array() noexcept { drop(); }
	Array& operator=(Array&& other) noexcept {
		return *new(drop(), Nat{}) Array{move(other)};
	}
	Array& operator=(const Array&) = delete;
	template<typename... Ts>
	[[nodiscard]] Bool
	emplace_back(Ts&&... args) noexcept {
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{forward<Ts>(args)...};
		m_length++;
		return true;
	}
	[[nodiscard]] Bool
	push_back(const T& value) noexcept
		requires CopyConstructible<T>
	{
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{value};
		m_length++;
		return true;
	}
	[[nodiscard]] Bool
	push_back(T&& value) noexcept
		requires MoveConstructible<T>
	{
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{move(value)};
		m_length++;
		return true;
	}
	void clear() noexcept {
		if (m_length == 0) return;
		for (Ulen i = m_length - 1; i < m_length; i--) m_data[i].~T();
		m_length = 0;
	}
	[[nodiscard]] constexpr T* data() noexcept { return m_data; }
	[[nodiscard]] constexpr const T* data() const noexcept { return m_data; }
	[[nodiscard]] constexpr T& operator[](Ulen i) noexcept { return m_data[i]; }
	[[nodiscard]] constexpr const T& operator[](Ulen i) const noexcept { return m_data[i]; }
	[[nodiscard]] constexpr Ulen length() const noexcept { return m_length; }
	[[nodiscard]] constexpr Ulen capacity() const noexcept { return m_capacity; }
	[[nodiscard]] Bool resize(Ulen size) noexcept {
		if (!reserve(size)) return false;
		m_length = size;
		return true;
	}
	[[nodiscard]] Bool reserve(Ulen length) noexcept {
		if (length < m_capacity) {
			return true;
		}
		Ulen capacity = 0;
		while (capacity < length) {
			capacity = ((capacity + 1) * 3) / 2;
		}
		void* data = m_allocator.allocate(capacity * sizeof(T));
		if (!data) {
			return false;
		}
		for (Ulen i = 0; i < m_length; i++) {
			new (reinterpret_cast<T*>(data) + i, Nat{}) T{move(m_data[i])};
		}
		m_allocator.deallocate(m_data, sizeof(T) * m_capacity);
		m_data = reinterpret_cast<T*>(data);
		m_capacity = capacity;
		return true;
	}
	Bool pop_back() noexcept {
		if (m_length) {
			m_data[--m_length].~T();
			return true;
		}
		return false;
	}
private:
	Array* drop() noexcept {
		clear();
		m_allocator.deallocate(m_data, m_capacity * sizeof(T));
		return this;
	}
	T* m_data;
	Ulen m_length;
	Ulen m_capacity;
	Allocator& m_allocator;
};

} // namespace Biron

#endif // BIRON_ARRAY_INL