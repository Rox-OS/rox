#ifndef BIRON_LEXER_H
#define BIRON_LEXER_H
#include <biron/util/string.h>
#include <biron/util/numeric.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

struct Token {
	enum class Kind : Uint8 {
		#define KIND(NAME) NAME,
		#include <biron/lexer.inl>
		#undef KIND
	};
	constexpr Token(Kind kind = Kind::END, Range range = {0, 0}) noexcept
		: kind{kind}
		, range{range}
	{
	}
	StringView name() const noexcept;
	Bool eof() const noexcept { return kind == Kind::END; }

	Sint32 binary_prec_() const noexcept {
		switch (kind) {
		// 0 is reserved
		// 1 will be used for '::'
		case Kind::KW_OF:    return 4;  // Query
		case Kind::KW_AS:    return 4;  // Cast                (LTR)
		case Kind::STAR:     return 5;  // Multiplication      (LTR)
		case Kind::FSLASH:   return 5;  // Division            (LTR)
		case Kind::PLUS:     return 6;  // Addition            (LTR)
		case Kind::MINUS:    return 6;  // Subtraction         (LTR)
		case Kind::LSHIFT:   return 7;  // Left shift          (LTR)
		case Kind::RSHIFT:   return 7;  // Right shift         (LTR)
		// 8 will be used for CMP
		case Kind::LT:       return 9;  // Less than           (LTR)
		case Kind::LTE:      return 9;  // Less than equal     (LTR)
		case Kind::GT:       return 9;  // Greater than        (LTR)
		case Kind::GTE:      return 9;  // Greater than equal  (LTR)
		case Kind::MIN:      return 9;  // Minimum             (LTR)
		case Kind::MAX:      return 9;  // Maximum             (LTR)
		case Kind::EQEQ:     return 10; // Equal               (LTR)
		case Kind::NEQ:      return 10; // Not equal           (LTR)
		case Kind::BAND:     return 11; // Bitwise AND         (LTR)
		// 12 will be used for Bitwise XOR
		case Kind::BOR:      return 13; // Bitwise OR          (LTR)
		case Kind::LAND:     return 14; // Logical AND         (LTR)
		case Kind::LOR:      return 15; // Logical OR          (LTR)

		// case Kind::EQ:       return 16; // Direct assignment   (RTL)
		// 16 will also be used for +=, -=, ..
		default:
			return 17;
		}
		BIRON_UNREACHABLE();
	}

	Sint32 unary_prec_() const noexcept {
		switch (kind) {
		case Kind::LPAREN:   return 2; // Function call (LTR)
		case Kind::RBRACKET: return 2; // Subscript     (LTR)
		case Kind::DOT:      return 2; // Member access (LTR)

		// RTL is handled elsewhere
		case Kind::KW_AS:    return 3; // Cast          (RTL)
		case Kind::NOT:      return 3; // Logical NOT   (RTL)
		case Kind::PLUS:     return 3; // Unary plus    (RTL)
		case Kind::MINUS:    return 3; // Unary minus   (RTL)
		case Kind::STAR:     return 4; // Indirection   (RTL)
		case Kind::BAND:     return 4; // Address-of    (RTL)
		default:
			return 17;
		}
		BIRON_UNREACHABLE();
	}

	Sint32 binary_prec() const noexcept { return 16 - binary_prec_(); }
	Sint32 unary_prec() const noexcept { return 16 - unary_prec_(); }

	Kind kind;
	Range range;
};

struct Lexer {
	constexpr Lexer(StringView name, StringView data) noexcept
		: m_name{name}
		, m_data{data}
		, m_offset{0}
	{
	}
	Token next() noexcept;
	constexpr StringView string(Range range) noexcept {
		return m_data.slice(range.offset, range.length);
	}
	constexpr const char& operator[](Ulen offset) const noexcept {
		return m_data[offset];
	}
	constexpr StringView name() const noexcept {
		return m_name;
	}
private:
	Token read() noexcept;
	Ulen fwd() noexcept { return m_offset++; }
	int peek() noexcept {
		return m_offset < m_data.length() ? m_data[m_offset] : -1;
	}
	StringView m_name;
	StringView m_data;
	Ulen m_offset;
};

} // namespace Biron

#endif // BIRON_LEXER_H