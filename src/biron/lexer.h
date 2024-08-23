#ifndef BIRON_LEXER_H
#define BIRON_LEXER_H
#include <biron/util/string.inl>
#include <biron/util/numeric.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

struct Token {
	enum class Kind {
		END,
		AT,        // '@'
		COMMA,     // ','
		COLON,     // ':'
		SEMI,      // ';'
		LPAREN,    // '('
		RPAREN,    // ')'
		LBRACKET,  // '['
		RBRACKET,  // ']'
		LBRACE,    // '{'
		RBRACE,    // '}'
		PLUS,      // '+'
		MINUS,     // '-'
		STAR,      // '*'
		PERCENT,   // '%'
		NOT,       // '!'
		DOLLAR,    // '$'
		BOR,       // '|'
		LOR,       // '||'
		BAND,      // '&'
		LAND,      // '&&'
		DOT,       // '.'
		SEQUENCE,  // '..'
		ELLIPSIS,  // '...'
		EQ,        // '='
		EQEQ,      // '=='
		NEQ,       // '!='
		LT,        // '<'
		LTE,       // '<='
		LSHIFT,    // '<<'
		GT,        // '>'
		GTE,       // '>='
		RSHIFT,    // '>>'
		ARROW,     // '->'
		IDENT,     // [a-z][A-Z]([a-z][A-Z][0-9]_)+

		KW_TRUE,   // true
		KW_FALSE,  // false

		KW_FN,       // 'fn'
		KW_IF,       // 'if'
		KW_AS,       // 'as'
		KW_LET,      // 'let'
		KW_FOR,      // 'for'
		KW_ASM,      // 'asm'
		KW_ELSE,     // 'else'
		KW_DEFER,    // 'defer'
		KW_UNION,    // 'union'
		KW_BREAK,    // 'break'
		KW_RETURN,   // 'return'
		KW_STRUCT,   // 'struct'
		KW_CONTINUE, // 'continue'

		LIT_INT,     // 0b[01]+(_(u|s){8,16,32,64})?
		             // 0x([0-9][a-f])+(_(u|s){8,16,32,64})?
		             // [1-9][0-9]+(_(u|s){8,16,32,64})?
		LIT_STR,     // ".*"
		LIT_CHR,     // '.*'

		COMMENT,
		UNKNOWN,
	};
	constexpr Token(Kind kind = Kind::END, Range range = {0, 0}) noexcept
		: kind{kind}
		, range{range}
	{
	}
	const char *name() const noexcept {
		switch (kind) {
		case Kind::END:         return "EOF";
		case Kind::AT:          return "AT";
		case Kind::COMMA:       return "COMMA";
		case Kind::COLON:       return "COLON";
		case Kind::SEMI:        return "SEMI";
		case Kind::LPAREN:      return "LPAREN";
		case Kind::RPAREN:      return "RPAREN";
		case Kind::LBRACKET:    return "LBRACKET";
		case Kind::RBRACKET:    return "RBRACKET";
		case Kind::LBRACE:      return "LBRACE";
		case Kind::RBRACE:      return "RBRACE";
		case Kind::DOT:         return "DOT";      // '.'
		case Kind::SEQUENCE:    return "SEQUENCE"; // '..'
		case Kind::ELLIPSIS:    return "ELLIPSIS"; // '...'
		case Kind::EQ:          return "EQ";
		case Kind::EQEQ:        return "EQEQ";
		case Kind::NEQ:         return "NEQ";
		case Kind::MINUS:       return "MINUS";
		case Kind::STAR:        return "STAR";
		case Kind::PERCENT:     return "PERCENT";
		case Kind::NOT:         return "NOT";
		case Kind::DOLLAR:      return "DOLLAR";
		case Kind::BOR:         return "BOR";
		case Kind::LOR:         return "LOR";
		case Kind::BAND:        return "BAND";
		case Kind::LAND:        return "LAND";
		case Kind::PLUS:        return "PLUS";
		case Kind::LT:          return "LT";
		case Kind::LTE:         return "LTE";
		case Kind::LSHIFT:      return "LSHIFT";
		case Kind::GT:          return "GT";
		case Kind::GTE:         return "GTE";
		case Kind::RSHIFT:      return "RSHIFT";
		case Kind::ARROW:       return "ARROW";
		case Kind::IDENT:       return "IDENT";
		case Kind::KW_TRUE:     return "TRUE";
		case Kind::KW_FALSE:    return "FALSE";
		case Kind::KW_FN:       return "FN";
		case Kind::KW_IF:       return "IF";
		case Kind::KW_AS:       return "AS";
		case Kind::KW_LET:      return "LET";
		case Kind::KW_FOR:      return "FOR";
		case Kind::KW_ASM:      return "ASM";
		case Kind::KW_ELSE:     return "ELSE";
		case Kind::KW_DEFER:    return "DEFER";
		case Kind::KW_UNION:    return "UNION";
		case Kind::KW_BREAK:    return "BREAK";
		case Kind::KW_RETURN:   return "RETURN";
		case Kind::KW_STRUCT:   return "STRUCT";
		case Kind::KW_CONTINUE: return "CONTINUE";
		case Kind::LIT_INT:     return "INT";
		case Kind::LIT_STR:     return "STR";
		case Kind::LIT_CHR:     return "CHR";
		case Kind::COMMENT:     return "COMMENT";
		case Kind::UNKNOWN:     return "UNKNOWN";
		}
		return "";
	}
	Bool eof() const noexcept { return kind == Kind::END; }
	Sint32 prec() const noexcept {
		switch (kind) {
		case Kind::LPAREN:   return 16 - 1;
		// case Kind::LBRACKET: return 16 - 1;
		case Kind::DOT:      return 16 - 1;
		case Kind::KW_AS:    return 16 - 1;
		// case Kind::LNOT:     return 16 - 2;
		// case Kind::BNOT:     return 16 - 2;
		// case Kind::STAR:     return 16 - 2;
		case Kind::STAR:     return 16 - 3;
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
	int operator[](Ulen offset) const noexcept {
		return m_data[offset];
	}
	StringView name() const noexcept {
		return m_name;
	}
private:
	Token read() noexcept;
	Ulen fwd() noexcept { return m_offset++; }
	int peek() {
		return m_offset < m_data.length() ? m_data[m_offset] : -1;
	}
	StringView m_name;
	StringView m_data;
	Ulen m_offset;
};

} // namespace Biron

#endif // BIRON_LEXER_H