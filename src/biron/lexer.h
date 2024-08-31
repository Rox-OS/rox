#ifndef BIRON_LEXER_H
#define BIRON_LEXER_H
#include <biron/util/string.inl>
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
	constexpr const char *name() const noexcept {
		switch (kind) {
		#define KIND(NAME) case Kind::NAME: return #NAME;
		#include <biron/lexer.inl>
		#undef KIND
		}
		return "";
	}
	Bool eof() const noexcept { return kind == Kind::END; }
	Sint32 prec() const noexcept {
		switch (kind) {
		case Kind::LPAREN:   return 16 - 1; // ()
		case Kind::LBRACKET: return 16 - 1; // []
		case Kind::DOT:      return 16 - 1; // .
		case Kind::KW_AS:    return 16 - 1; // as
		// case Kind::LNOT:     return 16 - 2;
		// case Kind::BNOT:     return 16 - 2;
		// case Kind::STAR:     return 16 - 2;
		case Kind::STAR:     return 16 - 3; // indirection (dereference)
		case Kind::PERCENT:  return 16 - 3; 
		case Kind::PLUS:     return 16 - 4;
		case Kind::MINUS:    return 16 - 4;
		case Kind::LSHIFT:   return 16 - 5;
		case Kind::RSHIFT:   return 16 - 5;
		case Kind::LT:       return 16 - 6;
		case Kind::LTE:      return 16 - 6;
		case Kind::GT:       return 16 - 6;
		case Kind::GTE:      return 16 - 6;
		case Kind::EQEQ:     return 16 - 7;
		case Kind::NEQ:      return 16 - 7;
		case Kind::BAND:     return 16 - 8;
		case Kind::BOR:      return 16 - 10;
		case Kind::LAND:     return 16 - 11;
		case Kind::LOR:      return 16 - 12;
		// TODO(dweiler): ternary
		default:
			return -1;
		}
		BIRON_UNREACHABLE();
	}
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
	const char& operator[](Ulen offset) const noexcept {
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