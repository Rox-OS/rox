#include <biron/lexer.h>

namespace Biron {

static Bool is_space(int ch) noexcept {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static Bool is_alpha(int ch) noexcept {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static Bool is_digit(int ch) noexcept {
	return ch >= '0' && ch <= '9';
}

static Bool is_bin(int ch) noexcept {
	return ch == '0' || ch == '1';
}

static Bool is_hex(int ch) noexcept {
	return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static Bool is_alnum(int ch) noexcept {
	return is_alpha(ch) || is_digit(ch);
}

const char* Token::name() const noexcept {
	switch (kind) {
	#define KIND(NAME) case Kind::NAME: return #NAME;
	#include <biron/lexer.inl>
	#undef KIND
	}
	return "";
}

Token Lexer::next() noexcept {
	using Kind = Token::Kind;
	auto token = read();
	while (token.kind == Kind::COMMENT) {
		token = read();
	}
	return token;
}

Token Lexer::read() noexcept {
	using Kind = Token::Kind;
	Ulen n = 0;
	while (peek() != -1 && is_space(peek())) fwd(); // Skip whitespace
	switch (peek()) {
	case -1:  return {Kind::END,      {fwd(), 0}};
	case '@': return {Kind::AT,       {fwd(), 1}};
	case ',': return {Kind::COMMA,    {fwd(), 1}};
	case ';': return {Kind::SEMI,     {fwd(), 1}};
	case ':': return {Kind::COLON,    {fwd(), 1}};
	case '(': return {Kind::LPAREN,   {fwd(), 1}};
	case ')': return {Kind::RPAREN,   {fwd(), 1}};
	case '[': return {Kind::LBRACKET, {fwd(), 1}};
	case ']': return {Kind::RBRACKET, {fwd(), 1}};
	case '{': return {Kind::LBRACE,   {fwd(), 1}};
	case '}': return {Kind::RBRACE,   {fwd(), 1}};
	case '+':
		n = fwd(); // Consume '+'
		if (peek() == '=') {
			return {Kind::PLUSEQ, {fwd(), 2}};
		} else {
			return {Kind::PLUS,   {n,     1}};
		}
		break;
	case '-':
		n = fwd(); // Consume '-'
		switch (peek()) {
		case '>': return {Kind::ARROW,   {fwd(), 2}};
		case '=': return {Kind::MINUSEQ, {fwd(), 2}};
		default:  return {Kind::MINUS,   {n,     1}};
		}
		break;
	case '*':
		n = fwd(); // Consume '*'
		if (peek() == '=') {
			return {Kind::STAREQ, {fwd(), 2}};
		} else {
			return {Kind::STAR,   {n,     1}};
		}
		break;
	case '%': return {Kind::PERCENT, {fwd(), 1}};
	case '$': return {Kind::DOLLAR,  {fwd(), 1}};
	case '|':
		n = fwd(); // Consume '|'
		if (peek() == '|') {
			fwd(); // Consume '|'
			return {Kind::LOR, {n, 2}};
		} else {
			return {Kind::BOR, {n, 1}};
		}
	case '&':
		n = fwd(); // Consume '&'
		if (peek() == '&') {
			fwd(); // Consume '&'
			return {Kind::LAND, {n, 2}};
		} else {
			return {Kind::BAND, {n, 1}};
		}
	case '.':
		n = fwd(); // Consume '.'
		if (peek() == '.') {
			fwd();   // Consume '.'
			if (peek() == '.') {
				fwd(); // Consume '.'
				return {Kind::ELLIPSIS, {n, 3}};
			} else {
				return {Kind::SEQUENCE, {n, 2}};
			}
		} else {
			return {Kind::DOT, {n, 1}};
		}
		break;
	case '!':
		n = fwd(); // Consume '!'
		if (peek() == '=') {
			fwd(); // Consume '='
			return {Kind::NEQ, {n, 3}};
		} else {
			return {Kind::NOT, {n, 1}};
		}
	case '=':
		n = fwd(); // Consume '='
		if (peek() == '=') {
			fwd(); // Consume '='
			return {Kind::EQEQ, {n, 2}};
		} else {
			return {Kind::EQ,   {n, 1}};
		}
		break;
	case '<':
		n = fwd(); // Consume '<'
		switch (peek()) {
		case '<': fwd(); return {Kind::LSHIFT, {n, 2}};
		case '=': fwd(); return {Kind::LTE,    {n, 2}};
		default:         return {Kind::LT,     {n, 1}};
		}
		break;
	case '>':
		n = fwd(); // Consume '>'
		switch (peek()) {
		case '>': fwd(); return {Kind::RSHIFT, {n, 2}};
		case '=': fwd(); return {Kind::GTE,    {n, 2}};
		default:         return {Kind::GT,     {n, 1}};
		}
		break;
	case '\'':
		n = fwd(); // Consume '\''
		while (peek() != -1 && peek() != '\'') fwd();
		fwd(); // Consume '\''
		return {Kind::LIT_CHR, {n, m_offset - n}};
	case '"':
		n = fwd(); // Consume '"'
		while (peek() != -1 && peek() != '"') {
			if (peek() == '\\') fwd();
			fwd(); // Consume '\'
		}
		fwd(); // Consume '"'
		return {Kind::LIT_STR, {n, m_offset - n}};
	case '/':
		n = fwd(); // Consume '/'
		switch (peek()) {
		case '/': // Single-line comment
			fwd(); // Consume '/'
			while (peek() != -1 && peek() != '\n') fwd();
			return {Kind::COMMENT, {n, m_offset - n}};
		case '*': // Multi-line comment
			fwd(); // Consume '*'
			for (int i = 1; i != 0 && peek() != -1; /**/) {
				switch (peek()) {
				case '/':
					fwd(); // Consume '/'
					if (peek() == '*') {
						fwd(); // Consume '*'
						i++; // Open comment (allow nesting)
					}
					break;
				case '*':
					fwd(); // Consume '*'
					if (peek() == '/') {
						fwd(); // Consume '/'
						i--; // Close comment
					}
					break;
				default:
					fwd(); // Consume
					break;
				}
			}
			return {Kind::COMMENT, {n, m_offset - n}};
		case '=':
			fwd(); // Consume '='
			return {Kind::FSLASHEQ, {n, 2}};
		default:
			return {Kind::FSLASH,  {n, 1}};
		}
		break;
	default:
		if (is_alpha(peek()) || peek() == '_') {
			n = fwd();
			while (peek() != -1 && (is_alnum(peek()) || peek() == '_')) fwd();
			Ulen l = m_offset - n;
			StringView ident = m_data.slice(n, l);
			switch (l) {
			case 2:
				/**/ if (ident == "fn")       return {Kind::KW_FN,       {n, 2}};
				else if (ident == "if")       return {Kind::KW_IF,       {n, 2}};
				else if (ident == "as")       return {Kind::KW_AS,       {n, 2}};
				else if (ident == "of")       return {Kind::KW_OF,       {n, 2}};
				break;
			case 3:
				/**/ if (ident == "let")      return {Kind::KW_LET,      {n, 3}};
				else if (ident == "for")      return {Kind::KW_FOR,      {n, 3}};
				break;
			case 4:
				/**/ if (ident == "else")     return {Kind::KW_ELSE,     {n, 4}};
				else if (ident == "type")     return {Kind::KW_TYPE,     {n, 4}};
				else if (ident == "true")     return {Kind::KW_TRUE,     {n, 4}};
				break;
			case 5:
				/**/ if (ident == "union")    return {Kind::KW_UNION,    {n, 5}};
				else if (ident == "defer")    return {Kind::KW_DEFER,    {n, 5}};
				else if (ident == "false")    return {Kind::KW_FALSE,    {n, 5}};
				else if (ident == "break")    return {Kind::KW_BREAK,    {n, 5}};
				break;
			case 6:
				/**/ if (ident == "return")   return {Kind::KW_RETURN,   {n, 6}};
				else if (ident == "module")   return {Kind::KW_MODULE,   {n, 6}};
				else if (ident == "import")   return {Kind::KW_IMPORT,   {n, 6}};
				break;
			case 8:
				/**/ if (ident == "continue") return {Kind::KW_CONTINUE, {n, 8}};
				break;
			}
			return {Kind::IDENT, {n, l}};
		} else if (is_digit(peek())) {
			// The use of ' is allowed in the digit as a separator but never two
			// separators next to each other and never the first character in a digit.
			auto z = peek() == '0';
			Ulen n = fwd(); // Consume digit
			Ulen s = 0;     // Encountered a separator
			Ulen d = 0;     // Encountered a dot
			Kind k = Kind::LIT_INT;
			if (z) {
				// Consumed a '0'
				switch (peek()) {
				case 'x':
					fwd(); // Consume 'x'
					while (peek() != -1 && (is_hex(peek()) || s)) fwd(), s = peek() == '\'';
					break;
				case 'b':
					fwd(); // Consume 'b'
					while (peek() != -1 && (is_bin(peek()) || s)) fwd(), s = peek() == '\'';
					break;
				case '.':
					// 0.\d+
					fwd(); // Consume '.'
					k = Kind::LIT_FLT;
					d = 1;
					goto L_dec;
				}
			} else {
L_dec:
				while (peek() != -1) {
					if (is_digit(peek())) {
						fwd(); // Consume \d
						continue;
					} else if (peek() == '\'') {
						fwd(); // Consume '
						s++;
						continue;
					} else if (d == 0 && peek() == '.') {
						fwd(); // Consume '.'
						d++;
						k = Kind::LIT_FLT;
						continue;
					} else {
						break;
					}
				}
			}

			if (peek() == 'e') {
				fwd(); // Consume 'e'
				k = Kind::LIT_FLT;
				if (peek() == '-' || peek() == '+') {
					fwd(); // Consume sign
				}
				while (is_digit(peek())) {
					fwd(); // Consume digit
				}
			}

			// The numeric literal can be typed with one of the following suffix
			//	_(u|s){8,16,32,64}
			if (peek() == '_') {
				fwd(); // Consume '_'
				if (peek() == 'u' || peek() == 's') {
					fwd(); // Consume 'u' or 's'
					switch (peek()) {
					/****/ case '8': fwd();                           // Consume '8'
					break; case '1': fwd(); if (peek() == '6') fwd(); // Consume '16'
					break; case '3': fwd(); if (peek() == '2') fwd(); // Consume '32'
					break; case '6': fwd(); if (peek() == '4') fwd(); // Consume '64'
					}
				} else if (peek() == 'f') {
					k = Kind::LIT_FLT;
					fwd(); // Consume 'f'
					switch (peek()) {
					/****/ case '3': fwd(); if (peek() == '2') fwd(); // Consume '32'
					break; case '6': fwd(); if (peek() == '4') fwd(); // Consume '64'
					}
				}
			}
			Ulen l = m_offset - n;
			return {k, {n, l}};
		}
	}
	return {Kind::UNKNOWN, {fwd(), 1}};
}

} // namespace Biron
