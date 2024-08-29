#ifndef BIRON_DIAGNOSTIC_H
#define BIRON_DIAGNOSTIC_H
#include <biron/util/numeric.inl>
#include <biron/util/format.inl>

namespace Biron {

struct Allocator;
struct Lexer;

struct Diagnostic {
	enum class Kind {
		WARNING,
		ERROR,
		FATAL,
	};

	constexpr Diagnostic(Lexer& lexer, Allocator& allocator) noexcept
		: m_lexer{lexer}
		, m_allocator{allocator}
	{
	}

	template<typename... Ts>
	void error(Range range, const char* message, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(range, Kind::ERROR, message);
		} else if (auto fmt = format(m_allocator, message, forward<Ts>(args)...)) {
			diagnostic(range, Kind::ERROR, fmt->data());
		} else {
			diagnostic(range, Kind::FATAL, "Out of memory");
		}
	}

	template<typename... Ts>
	void fatal(Range range, const char* message, Ts&&... args) noexcept {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(range, Kind::FATAL, message);
		} else if (auto fmt = format(m_allocator, message, forward<Ts>(args)...)) {
			diagnostic(range, Kind::FATAL, fmt->data());
		} else {
			diagnostic(range, Kind::FATAL, "Out of memory");
		}
	}

	void diagnostic(Range range, Kind kind, const char *message) noexcept;

private:
	Lexer&     m_lexer;
	Allocator& m_allocator;
};

} // namespace Biron

#endif // BIRON_DIAGNOSTIC_H