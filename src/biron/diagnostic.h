#ifndef BIRON_DIAGNOSTIC_H
#define BIRON_DIAGNOSTIC_H
#include <biron/util/format.h>
#include <biron/util/numeric.inl>

namespace Biron {

struct Allocator;
struct Lexer;
struct Terminal;

struct Diagnostic {
	enum class Kind {
		WARNING,
		ERROR,
		FATAL,
	};

	constexpr Diagnostic(Lexer& lexer, Terminal& terminal, Allocator& allocator) noexcept
		: m_lexer{lexer}
		, m_terminal{terminal}
		, m_scratch{allocator}
	{
	}

	template<typename... Ts>
	void error(Range range, StringView message, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(range, Kind::ERROR, message);
		} else if (auto fmt = format(m_scratch, message, forward<Ts>(args)...)) {
			StringView view{fmt->data(), fmt->length()};
			diagnostic(range, Kind::ERROR, view);
		} else {
			diagnostic(range, Kind::FATAL, "Out of memory");
		}
	}

	template<typename... Ts>
	void fatal(Range range, StringView message, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(range, Kind::FATAL, message);
		} else if (auto fmt = format(m_scratch, message, forward<Ts>(args)...)) {
			StringView view{fmt->data(), fmt->length()};
			diagnostic(range, Kind::FATAL, view);
		} else {
			diagnostic(range, Kind::FATAL, "Out of memory");
		}
	}

	void diagnostic(Range range, Kind kind, StringView message) noexcept;

private:
	Lexer&           m_lexer;
	Terminal&        m_terminal;
	ScratchAllocator m_scratch;
};

} // namespace Biron

#endif // BIRON_DIAGNOSTIC_H