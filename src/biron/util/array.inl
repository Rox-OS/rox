#ifndef BIRON_ARRAY_INL
#define BIRON_ARRAY_INL
#include <biron/util/allocator.inl>
#include <biron/util/numeric.inl>
#include <biron/util/maybe.inl>
#include <biron/util/traits/conditional.inl>

namespace Biron {

template<typename T>
struct Array {
	template<Bool B>
	using SelectIterator = Conditional<const T*, T*, B>;

	using Iterator       = SelectIterator<false>;
	using ConstIterator  = SelectIterator<true>;

	constexpr Array(Allocator& allocator) noexcept
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
	Array& operator=(Array&& other) noexcept {
		return *new(drop(), Nat{}) Array{move(other)};
	}

	Array& operator=(const Array&) noexcept = delete;
	constexpr Array(const Array&) noexcept = delete;

	~Array() noexcept { drop(); }

	template<typename... Ts>
	[[nodiscard]] Bool emplace_back(Ts&&... args) noexcept {
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{forward<Ts>(args)...};
		m_length++;
		return true;
	}

	[[nodiscard]] Bool push_back(const T& value) noexcept
		requires CopyConstructible<T>
	{
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{value};
		m_length++;
		return true;
	}

	[[nodiscard]] Bool push_back(T&& value) noexcept
		requires MoveConstructible<T>
	{
		if (!reserve(m_length + 1)) return false;
		new (m_data + m_length, Nat{}) T{move(value)};
		m_length++;
		return true;
	}

	void clear() noexcept {
		destruct();
		m_length = 0;
	}

	void reset() noexcept {
		drop();
		m_length = 0;
		m_capacity = 0;
	}

	[[nodiscard]] constexpr T* data() noexcept { return m_data; }
	[[nodiscard]] constexpr const T* data() const noexcept { return m_data; }

	[[nodiscard]] constexpr T& first() noexcept { return m_data[0]; }
	[[nodiscard]] constexpr const T& first() const noexcept { return m_data[0]; }
	[[nodiscard]] constexpr T& last() noexcept { return m_data[m_length - 1]; }
	[[nodiscard]] constexpr const T& last() const noexcept { return m_data[m_length - 1]; }

	[[nodiscard]] constexpr T& operator[](Ulen i) noexcept { return m_data[i]; }
	[[nodiscard]] constexpr const T& operator[](Ulen i) const noexcept { return m_data[i]; }
	[[nodiscard]] constexpr T* at(Ulen i) noexcept { return i < m_length ? m_data + i : nullptr; }
	[[nodiscard]] constexpr const T* at(Ulen i) const noexcept { return i < m_length ? m_data + i : nullptr; }

	[[nodiscard]] constexpr Ulen length() const noexcept { return m_length; }
	[[nodiscard]] constexpr Ulen capacity() const noexcept { return m_capacity; }
	[[nodiscard]] constexpr Bool empty() const noexcept { return m_length == 0; }

	[[nodiscard]] constexpr Iterator begin() noexcept { return m_data; }
	[[nodiscard]] constexpr Iterator end() noexcept { return m_data + m_length; }
	[[nodiscard]] constexpr ConstIterator begin() const noexcept { return m_data; }
	[[nodiscard]] constexpr ConstIterator end() const noexcept { return m_data + m_length; }

	[[nodiscard]] constexpr Allocator& allocator() const noexcept { return m_allocator; }

	[[nodiscard]] Bool resize(Ulen size) noexcept {
		if (size < m_length) {
			// Call the destructors on objects from the tail until we reach 'size'
			for (Ulen i = m_length - 1; i > size; i--) {
				m_data[i].~T();
			}
		} else if (size > m_length) {
			if (!reserve(size)) {
				// Out of memory.
				return false;
			}
			// When growing we must default construct the new elements.
			for (Ulen i = m_length; i < size; i++) {
				new (m_data + i, Nat{}) T;
			}
		}
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
		drop();
		m_data = reinterpret_cast<T*>(data);
		m_capacity = capacity;
		return true;
	}

	Bool pop_back() noexcept {
		if (m_length == 0) {
			return false;
		}
		m_data[--m_length].~T();
		return true;
	}

	[[nodiscard]] constexpr Bool operator==(const Array& other) const noexcept {
		if (&other == this) {
			// Simple identity will always compare true.
			return true;
		}
		if (other.m_length != m_length) {
			// The lengths must match.
			return false;
		}
		// The capacities do not need to match though.
		for (Ulen l = m_length, i = 0; i < l; i++) {
			if (other[i] != operator[](i)) {
				// The elements must match though.
				return false;
			}
		}
		return false;
	}

	Maybe<Array> copy() const noexcept
		requires CopyConstructible<T> || MaybeCopyable<T>;

private:
	void destruct() noexcept {
		// Call destructors in reverse array order which is what people expect.
		for (Ulen i = m_length - 1; i < m_length; i--) {
			m_data[i].~T();
		}
	}

	Array* drop() noexcept {
		destruct();
		m_allocator.deallocate(m_data, m_capacity * sizeof(T));
		return this;
	}

	T* m_data;
	Ulen m_length;
	Ulen m_capacity;
	Allocator& m_allocator;
};

template<typename T>
Maybe<Array<T>> Array<T>::copy() const noexcept
	requires CopyConstructible<T> || MaybeCopyable<T>
{
	Array<T> result{allocator()};
	if (!result.reserve(length())) {
		return None{};
	}
	for (const auto &ith : *this) {
		if constexpr(MaybeCopyable<T>) {
			auto elem = ith.copy();
			if (!elem || !result.push_back(move(*elem))) {
				return None{};
			}
		} else if (!result.push_back(ith)) {
			// CopyConstructible
			return None{};
		}
	}
	return result;
}

} // namespace Biron

#endif // BIRON_ARRAY_INL