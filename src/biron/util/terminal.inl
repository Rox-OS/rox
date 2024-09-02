#ifndef BIRON_TERMINAL_INL
#define BIRON_TERMINAL_INL
#include <biron/util/format.h>
#include <biron/util/system.inl>

namespace Biron {

struct Terminal {
	constexpr Terminal(const System& system) noexcept
		: m_system{system}
		, m_allocator{system}
		, m_scratch{m_allocator}
	{
	}

	template<typename... Ts>
	void out(StringView fmt, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			m_system.term_out(m_system, fmt);
		} else if (const auto msg = format(m_scratch, fmt, forward<Ts>(args)...)) {
			const auto view = StringView{msg->data(), msg->length()};
			m_system.term_out(m_system, view);
		} else {
			m_system.term_out(m_system, "Out of memory");
		}
	}

	template<typename... Ts>
	void err(StringView fmt, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			m_system.term_err(m_system, fmt);
		} else if (const auto msg = format(m_scratch, fmt, forward<Ts>(args)...)) {
			const auto view = StringView{msg->data(), msg->length()};
			m_system.term_err(m_system, view);
		} else {
			m_system.term_err(m_system, "Out of memory");
		}
	}

private:
	const System& m_system;
	SystemAllocator m_allocator;
	ScratchAllocator m_scratch;
};

} // namespace Biron

#endif // BIRON_TERMINAL_INL