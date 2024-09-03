#include <biron/parser.h>

#include <biron/ast_attr.h>
#include <biron/ast_const.h>
#include <biron/ast_expr.h>
#include <biron/ast_stmt.h>
#include <biron/ast_type.h>

#include <stdlib.h> // strtoul, strtod
#include <string.h> // strncmp
#include <errno.h>  // errno, ERANGE

#define ERROR(...) \
	error(__VA_ARGS__)

namespace Biron {

Bool Scope::find(StringView name) const noexcept {
	// Just assume printf exists for debugging purposes.
	if (name == "printf") {
		return true;
	}
	// Search let declarations
	for (auto let : m_lets) {
		if (let->name() == name) {
			return true;
		}
	}
	// Search fn declarations
	for (auto fn : m_fns) {
		if (fn->name() == name) {
			return true;
		}
	}
	return false;
}

Bool Scope::add_fn(AstFn* fn) noexcept {
	return m_fns.push_back(fn);
}

Bool Scope::add_let(AstLetStmt* let) noexcept {
	return m_lets.push_back(let);
}

Parser::~Parser() noexcept {
	for (auto& cache : m_caches) {
		for (auto node : cache) {
			static_cast<AstNode*>(node)->~AstNode();
		}
	}
}

AstExpr* Parser::parse_index_expr(AstExpr* operand) noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		ERROR("Expected '['");
		return nullptr;
	}
	auto beg_token = peek();
	next(); // Consume '['
	auto index = parse_expr(false);
	if (!index) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::RBRACKET) {
		ERROR("Expected ']'");
		return nullptr;
	}
	auto end_token = next(); // Consume ']'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstIndexExpr>(operand, index, range);
}

AstExpr* Parser::parse_binop_rhs(Bool simple, int expr_prec, AstExpr* lhs) noexcept {
	using Op = AstBinExpr::Op;
	for (;;) {
		auto peek_prec = peek().binary_prec();
		if (peek_prec < expr_prec) {
			return lhs;
		}
		auto token = next();
		auto kind = token.kind; // Eat Op
		AstExpr* rhs = nullptr;
		if (kind == Token::Kind::KW_AS) {
			rhs = parse_type_expr();
		} else {
			rhs = parse_unary_expr(simple);
		}
		if (!rhs) {
			return nullptr;
		}
		// Handle less-tight binding for non-unary
		auto next_prec = peek().binary_prec();
		if (peek_prec < next_prec) {
			rhs = parse_binop_rhs(simple, peek_prec + 1, rhs);
			if (!rhs) {
				return nullptr;
			}
		}
		// Map the token 'kind' to an AstBin::Operator
		auto range = token.range.include(lhs->range())
		                        .include(rhs->range());
		switch (kind) {
		/***/  case Token::Kind::KW_AS:  lhs = new_node<AstBinExpr>(Op::AS,     lhs, rhs, range);
		break; case Token::Kind::KW_OF:  lhs = new_node<AstBinExpr>(Op::OF,     lhs, rhs, range);
		break; case Token::Kind::STAR:   lhs = new_node<AstBinExpr>(Op::MUL,    lhs, rhs, range);
		break; case Token::Kind::FSLASH: lhs = new_node<AstBinExpr>(Op::DIV,    lhs, rhs, range);
		break; case Token::Kind::PLUS:   lhs = new_node<AstBinExpr>(Op::ADD,    lhs, rhs, range);
		break; case Token::Kind::MINUS:  lhs = new_node<AstBinExpr>(Op::SUB,    lhs, rhs, range);
		break; case Token::Kind::LSHIFT: lhs = new_node<AstBinExpr>(Op::LSHIFT, lhs, rhs, range);
		break; case Token::Kind::RSHIFT: lhs = new_node<AstBinExpr>(Op::RSHIFT, lhs, rhs, range);
		break; case Token::Kind::LT:     lhs = new_node<AstBinExpr>(Op::LT,     lhs, rhs, range);
		break; case Token::Kind::LTE:    lhs = new_node<AstBinExpr>(Op::LE,     lhs, rhs, range);
		break; case Token::Kind::GT:     lhs = new_node<AstBinExpr>(Op::GT,     lhs, rhs, range);
		break; case Token::Kind::GTE:    lhs = new_node<AstBinExpr>(Op::GE,     lhs, rhs, range);
		break; case Token::Kind::EQEQ:   lhs = new_node<AstBinExpr>(Op::EQ,     lhs, rhs, range);
		break; case Token::Kind::NEQ:    lhs = new_node<AstBinExpr>(Op::NE,     lhs, rhs, range);
		break; case Token::Kind::BAND:   lhs = new_node<AstBinExpr>(Op::BAND,   lhs, rhs, range);
		break; case Token::Kind::BOR:    lhs = new_node<AstBinExpr>(Op::BOR,    lhs, rhs, range);
		break; case Token::Kind::LAND:   lhs = new_node<AstBinExpr>(Op::LAND,   lhs, rhs, range);
		break; case Token::Kind::LOR:    lhs = new_node<AstBinExpr>(Op::LOR,    lhs, rhs, range);
		break;
		default:
			ERROR(token.range, "Unexpected token '%s' in binary expression", token.name());
			return nullptr;
		}
	}
}

// PostfixExpr
//	::= IndexExpr
//	  | DotExpr
AstExpr* Parser::parse_postfix_expr(Bool simple) noexcept {
	auto operand = parse_primary_expr(simple);
	if (!operand) {
		return nullptr;
	}
	for (;;) {
		switch (peek().kind) {
		case Token::Kind::DOT:
			{
				next(); // Consume '.'
				auto expr = parse_primary_expr(false);
				if (!expr) {
					return nullptr;
				}
				auto range = operand->range().include(expr->range());
				operand = new_node<AstBinExpr>(AstBinExpr::Op::DOT, operand, expr, range);
			}
			break;
		case Token::Kind::LBRACKET:
			if (!(operand = parse_index_expr(operand))) {
				return nullptr;
			}
			break;
		default:
			return operand;
		}
	}
	return nullptr;
}

// UnaryExpr
//	::= PostfixExpr
//	  | '!' UnaryExpr
//	  | '-' UnaryExpr
//	  | '+' UnaryExpr
//	  | '*' UnaryExpr
//	  | '&' UnaryExpr
//	  | '...' UnaryExpr
AstExpr* Parser::parse_unary_expr(Bool simple) noexcept {
	using Op = AstUnaryExpr::Op;
	AstExpr* operand = nullptr;
	auto token = peek();
	switch (token.kind) {
	case Token::Kind::NOT:
		next(); // Consume '!'
		if (!(operand = parse_unary_expr(simple))) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::NOT, operand, token.range.include(operand->range()));
	case Token::Kind::MINUS:
		next(); // Consume '-'
		if (!(operand = parse_unary_expr(simple))) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::NEG, operand, token.range.include(operand->range()));
	case Token::Kind::PLUS:
		next(); // Consume '+'
		return parse_unary_expr(simple);
	case Token::Kind::STAR:
		next(); // Consume '*'
		if (!(operand = parse_unary_expr(simple))) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::DEREF, operand, token.range.include(operand->range()));
	case Token::Kind::BAND:
		next(); // Consume '&'
		if (!(operand = parse_unary_expr(simple))) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::ADDROF, operand, token.range.include(operand->range()));
	case Token::Kind::ELLIPSIS:
		next();
		if (!(operand = parse_unary_expr(simple))) {
			return nullptr;
		}
		return new_node<AstExplodeExpr>(operand, token.range.include(operand->range()));
	default:
		return parse_postfix_expr(simple);
	}
	BIRON_UNREACHABLE();
}

// Expr
//	::= PrimaryExpr BinOpRHS
//	  | PrimaryExpr IndexExpr
AstExpr* Parser::parse_expr(Bool simple) noexcept {
	auto lhs = parse_unary_expr(simple);
	if (!lhs) {
		return nullptr;
	}
	return parse_binop_rhs(simple, 0, lhs);
}

// PrimaryExpr
//	::= IdentExpr
//	  | BoolExpr
//	  | IntExpr
//	  | StrExpr
//	  | TupleExpr
//	  | TypeExpr
//	  | AggExpr
AstExpr* Parser::parse_primary_expr(Bool simple) noexcept {
	switch (peek().kind) {
	case Token::Kind::IDENT:
		{
			auto ident = parse_ident_expr(simple);
			if (!simple && ident->is_expr<AstTypeExpr>()) {
				return parse_agg_expr(ident);
			} else {
				return ident;
			}
		}
		break;
	case Token::Kind::LBRACKET:
		if (!simple) {
			// Can be either an AstTypeExpr as in [N]T
			// Or can be an AstArrayExpr as in [N]T { ... }
			if (auto type = parse_type_expr()) {
				return parse_agg_expr(type);
			} else {
				return nullptr;
			}
		}
		break;
	case Token::Kind::LBRACE:
		if (!simple) {
			// We're parsing an aggregate initializer but we have no type so it will need
			// to be inferred. This can only be a structure or array type.
			//
			// This currently does not work since we lack bi-directional type inference,
			// and only support forward inferencing. So any use of this aggregate will
			// likely crash the compiler right now.
			//
			// TODO(dweiler): Revisit.
			return parse_agg_expr(nullptr);
		}
		break;
	case Token::Kind::KW_TRUE:
	case Token::Kind::KW_FALSE:
		return parse_bool_expr();
	case Token::Kind::LIT_INT:
		return parse_int_expr();
	case Token::Kind::LIT_FLT:
		return parse_flt_expr();
	case Token::Kind::LIT_STR:
		return parse_str_expr();
	case Token::Kind::LIT_CHR:
		return parse_chr_expr();
	case Token::Kind::LPAREN:
		return parse_tuple_expr();
	case Token::Kind::STAR:
		return parse_type_expr();
	default:
		break;
	}
	ERROR("Unknown token '%s' in primary expression", peek().name());
	return nullptr;
}

AstExpr* Parser::parse_agg_expr(AstExpr* type_expr) noexcept {
	AstType* type = nullptr;
	if (type_expr) {
		if (!type_expr->is_expr<AstTypeExpr>()) {
			return nullptr;
		}
		type = static_cast<AstTypeExpr*>(type_expr)->type();
	}
	if (peek().kind != Token::Kind::LBRACE) {
		return type_expr;
	}
	Array<AstExpr*> exprs{m_allocator};
	next(); // Skip '{'
	while (peek().kind != Token::Kind::RBRACE) {
		auto expr = parse_expr(false);
		if (!expr || !exprs.push_back(expr)) {
			return nullptr;
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::RBRACE) {
		ERROR("Expected '}'");
		return nullptr;
	}
	auto end_token = next(); // Skip '}'
	auto range = type->range().include(end_token.range);
	return new_node<AstAggExpr>(type, move(exprs), range);
}

// TypeExpr
//	::= Type
AstExpr* Parser::parse_type_expr() noexcept {
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstTypeExpr>(type, type->range());
}

// IdentExpr
//	::= CallExpr
//    | AggExpr
// CallExpr
//	::= Ident TupleExpr?
AstExpr* Parser::parse_ident_expr(Bool simple) noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}

	auto token = next(); // Consume ident
	switch (peek().kind) {
	case Token::Kind::LPAREN: // ()
		{
			auto args = parse_tuple_expr();
			if (!args) {
				return nullptr;
			}
			auto name = m_lexer.string(token.range);
			auto expr = new_node<AstVarExpr>(name, token.range);
			return new_node<AstCallExpr>(expr, args, name == "printf", token.range.include(args->range()));
		}
		break;
	case Token::Kind::LBRACE: // {}
		if (!simple) {
			auto name = m_lexer.string(token.range);
			auto type = new_node<AstIdentType>(name, None{}, token.range);
			if (!type) {
				return nullptr;
			}
			auto expr = new_node<AstTypeExpr>(type, token.range);
			if (!expr) {
				return nullptr;
			}
			return parse_agg_expr(expr);
		}
		[[fallthrough]];
	default:
		{
			auto name = m_lexer.string(token.range);
			return new_node<AstVarExpr>(name, token.range);
		}
	}
	return nullptr;
}

// IntExpr
//	::= '0x' (HexDigit DigitSep?)+
//	  | '0b' (BinDigit DigitSep?)+
//	  | (DecDigit DigitSep?)+
// DigitSep
//	::= "'"
AstIntExpr* Parser::parse_int_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_INT) {
		ERROR("Expected int literal");
		return nullptr;
	}

	auto token = next();

	// Filter out digit separator '
	StringBuilder builder{m_allocator};
	auto lit = m_lexer.string(token.range);
	for (Ulen l = lit.length(), i = 0; i < l; i++) {
		if (lit[i] != '\'') {
			builder.append(lit[i]);
		} else if (i == l - 1 || lit[i + 1] == '_') {
			// The integer literal should not end with trailing ' or '_T
			auto skip = token.range;
			skip.offset += i;
			skip.length -= i;
			ERROR(skip, "Unexpected trailing digits separator in integer literal");
			return nullptr;
		}
	}
	builder.append('\0');
	if (!builder.valid()) {
		ERROR("Out of memory");
		return nullptr;
	}

	// Use strtoull on filtered literal.
	char* buf = builder.data();
	char* end = nullptr;
	Uint64 n = 0;
	if (lit.starts_with("0x")) {
		n = strtoull(buf, &end, 16);
	} else if (lit.starts_with("0b")) {
		n = strtoull(buf, &end, 2);
	} else {
		n = strtoull(buf, &end, 10);
	}

	// Check that the result of strtoull is within bounds of requested literal
	// type with a table.
	static constexpr const struct {
		Uint8      id;
		StringView type;
		StringView name;
		Uint64     max;
	} TABLE[] = {
		{ 0, "_u8",  "Uint8",  0xff_u64                  },
		{ 1, "_u16", "Uint16", 0xffff_u64                },
		{ 2, "_u32", "Uint32", 0xffff'ffff_u64           },
		{ 3, "_u64", "Uint64", 0xffff'ffff'ffff'ffff_u64 },
		{ 4, "_s8",  "Sint8",  0x7f_u64                  },
		{ 5, "_s16", "Sint16", 0x7fff_u64                },
		{ 6, "_s32", "Sint32", 0x7fff'ffff_u64           },
		{ 7, "_s64", "Sint64", 0x7fff'ffff'ffff'ffff_u64 },
	};
	for (const auto& match : TABLE) {
		if (strncmp(end, match.type.data(), match.type.length()) != 0) {
			continue;
		}
		if (errno == ERANGE || n > match.max) {
			ERROR("Integer literal '%S' too large", match.name);
			return nullptr;
		}
		// Control flow inversion since no runtime typing or compile-time for loop.
		switch (match.id) {
		case 0: return new_node<AstIntExpr>(Uint8(n),  token.range);
		case 1: return new_node<AstIntExpr>(Uint16(n), token.range);
		case 2: return new_node<AstIntExpr>(Uint32(n), token.range);
		case 3: return new_node<AstIntExpr>(Uint64(n), token.range);
		case 4: return new_node<AstIntExpr>(Sint8(n),  token.range);
		case 5: return new_node<AstIntExpr>(Sint16(n), token.range);
		case 6: return new_node<AstIntExpr>(Sint32(n), token.range);
		case 7: return new_node<AstIntExpr>(Sint64(n), token.range);
		default:
			BIRON_UNREACHABLE();
		}
	}

	return new_node<AstIntExpr>(AstIntExpr::Untyped{n}, token.range);
}

// ChrExpr
//	::= Byte
AstIntExpr* Parser::parse_chr_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_CHR) {
		ERROR("Expected character literal");
		return nullptr;
	}
	auto token = next(); // Consume CHR
	auto literal = m_lexer.string(token.range);
	// The lexer retains the quotes in the string so that token.range is accurate,
	// here we just slice them off.
	literal = literal.slice(1, literal.length() - 2);
	return new_node<AstIntExpr>(Uint8(literal[0]), token.range);
}

// FltExpr
//	::= (DecDigit DigitSep?)+
AstFltExpr* Parser::parse_flt_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_FLT) {
		ERROR("Expected float literal");
		return nullptr;
	}

	auto token = next();

	// Filter out digit separator '
	StringBuilder builder{m_allocator};
	auto lit = m_lexer.string(token.range);
	for (Ulen l = lit.length(), i = 0; i < l; i++) {
		if (lit[i] != '\'') {
			builder.append(lit[i]);
		} else if (i == l - 1 || lit[i + 1] == '_') {
			// The floating-point literal should not end with trailing ' or '_T
			auto skip = token.range;
			skip.offset += i;
			skip.length -= i;
			ERROR(skip, "Unexpected trailing digits separator in floating-point literal");
			return nullptr;
		}
	}
	builder.append('\0');
	if (!builder.valid()) {
		ERROR("Out of memory");
		return nullptr;
	}
	char* end = nullptr;
	auto value = strtod(builder.data(), &end);
	if (!strncmp(end, "_f64", 3)) {
		const Float64 v = value;
		return new_node<AstFltExpr>(v, token.range);
	} else if (!strncmp(end, "_f32", 3)) {
		const Float32 v = value;
		return new_node<AstFltExpr>(v, token.range);
	}
	return new_node<AstFltExpr>(AstFltExpr::Untyped{value}, token.range);
}

// StrExpr
//	::= '"' .* '"'
AstStrExpr* Parser::parse_str_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_STR) {
		ERROR("Expected string literal");
		return nullptr;
	}
	auto token = next();
	auto literal = m_lexer.string(token.range);
	// The lexer retains the quotes in the string so that token.range is accurate,
	// here we just slice them off.
	literal = literal.slice(1, literal.length() - 2);
	return new_node<AstStrExpr>(literal, token.range);
}

// BoolExpr
//	::= true
//	  | false
AstBoolExpr* Parser::parse_bool_expr() noexcept {
	if (peek().kind != Token::Kind::KW_TRUE &&
	    peek().kind != Token::Kind::KW_FALSE)
	{
		ERROR("Expected 'true' or 'false'");
	}
	auto token = next();
	if (token.kind == Token::Kind::KW_TRUE) {
		return new_node<AstBoolExpr>(true, token.range);
	} else {
		return new_node<AstBoolExpr>(false, token.range);
	}
	BIRON_UNREACHABLE();
}

// TupleExpr
//	::= '(' Expr (',' Expr)* ')'
AstTupleExpr* Parser::parse_tuple_expr() noexcept {
	auto beg_token = peek();
	if (beg_token.kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return nullptr;
	}
	next(); // Consume '('
	Array<AstExpr*> exprs{m_allocator};
	while (peek().kind != Token::Kind::RPAREN) {
		auto expr = parse_expr(false);
		if (!expr) {
			return nullptr;
		}
		if (!exprs.push_back(expr)) {
			ERROR("Out of memory");
			return nullptr;
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
			if (peek().kind == Token::Kind::RPAREN) {
				ERROR("Expected expression");
				return nullptr;
			}
		} else {
			break;
		}
	}
	auto end_token = peek();
	if (end_token.kind != Token::Kind::RPAREN) {
		ERROR("Expected ')' to terminate tuple expression");
		return nullptr;
	}
	next(); // Consume ')'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstTupleExpr>(move(exprs), range);
}

// Type
//	::= IdentType
//	  | TupleType
//	  | VarArgsType
//	  | PtrType
//	  | ArrayType
//	  | SliceType
//	  | FnType
//	  | UnionType
// UnionType
//	::= Type '|' Type ('|' Type)*
AstType* Parser::parse_type() noexcept {
	Array<AstType*> types{m_allocator};
	Maybe<Array<AstAttr*>> attrs;
	for (;;) {
		if (peek().kind == Token::Kind::AT) {
			attrs = parse_attrs();
			if (!attrs) {
				return nullptr;
			}
		}
		AstType* type = nullptr;
		switch (peek().kind) {
		case Token::Kind::IDENT:
			type = parse_ident_type(move(attrs));
			break;
		case Token::Kind::LPAREN:
			type = parse_tuple_type(move(attrs));
			break;
		case Token::Kind::ELLIPSIS:
			type = parse_varargs_type(move(attrs));
			break;
		case Token::Kind::STAR:
			type = parse_ptr_type(move(attrs));
			break;
		case Token::Kind::LBRACKET:
			type = parse_bracket_type(move(attrs));
			break;
		case Token::Kind::KW_FN:
			type = parse_fn_type(move(attrs));
			break;
		default:
			ERROR("Unexpected token '%s' in type", peek().name());
			return nullptr;
		}
		if (!type || !types.push_back(type)) {
			return nullptr;
		}
		if (peek().kind == Token::Kind::BOR) {
			next(); // Consume '|'
		} else {
			break;
		}
	}
	if (types.length() == 1) {
		return types[0];
	} else {
		auto range = types[0]->range();
		for (auto type : types) {
			range = range.include(type->range());
		}
		return new_node<AstUnionType>(move(types), move(attrs), range);
	}
	BIRON_UNREACHABLE();
}

// IdentType
//	::= Ident
AstIdentType* Parser::parse_ident_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range);
	return new_node<AstIdentType>(name, move(attrs), token.range);
}

// TupleType
//	::= '(' ')'
//	  | '(' TupleElem (',' TupleElem)* ')'
// TupleElem
//	::= (Ident ':')? Type
AstTupleType* Parser::parse_tuple_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return nullptr;
	}
	auto beg_token = next(); // Consume '('
	Array<AstTupleType::Elem> elems{m_allocator};
	while (peek().kind != Token::Kind::RPAREN) {
		// When we have LPAREN we have a nested tuple.
		if (peek().kind == Token::Kind::LPAREN) {
			auto nested = parse_tuple_type(None{});
			if (!nested) {
				return nullptr;
			}
			if (!elems.emplace_back(None{}, nested)) {
				ERROR("Out of memory");
				return nullptr;
			}
			if (peek().kind == Token::Kind::COMMA) {
				next(); // Consume ','
			}
			continue;
		}
		Maybe<Token> token;
		if (peek().kind == Token::Kind::IDENT) {
			token = next(); // Consume ident
		}
		if (peek().kind == Token::Kind::COLON) {
			if (!token) {
				ERROR("Expected identifier");
				return nullptr;
			}
			next(); // Consume ':'
			auto type = parse_type();
			if (!type) {
				return nullptr;
			}
			auto name = m_lexer.string(token->range);
			if (!elems.emplace_back(name, type)) {
				ERROR("Out of memory");
				return nullptr;
			}
		} else {
			AstType* type = nullptr;
			if (token) {
				auto name = m_lexer.string(token->range);
				type = new_node<AstIdentType>(name, None{}, token->range);
			} else {
				type = parse_type();
				if (!type) {
					return nullptr;
				}
			}
			if (!elems.emplace_back(None{}, type)) {
				ERROR("Out of memory");
				return nullptr;
			}
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
			if (peek().kind == Token::Kind::RPAREN) {
				ERROR("Expected element");
				return nullptr;
			}
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::RPAREN) {
		ERROR("Expected ')' to terminate tuple type");
		return nullptr;
	}
	auto end_token = next(); // Consume ')'
	return new_node<AstTupleType>(move(elems), move(attrs), beg_token.range.include(end_token.range));
}

// VarArgsType
//	::= '...'
AstVarArgsType* Parser::parse_varargs_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::ELLIPSIS) {
		ERROR("Expected '...'");
		return nullptr;
	}
	auto token = next(); // Consume '...'
	return new_node<AstVarArgsType>(move(attrs), token.range);
}

// PtrType
//	::= '*' Type
AstPtrType* Parser::parse_ptr_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::STAR) {
		ERROR("Expected '*'");
		return nullptr;
	}
	auto token = next(); // Consume '*'
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstPtrType>(type, move(attrs), token.range.include(type->range()));
}

// BracketType
//	::= ArrayType
//	  | SliceType
// ArrayType
//	::= '[' Expr ']' Type
// SliceType
//	::= '[' ']'
AstType* Parser::parse_bracket_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		ERROR("Expected '['");
		return nullptr;
	}
	auto beg_token = next(); // Consume '['
	AstExpr* expr = nullptr;
	if (peek().kind != Token::Kind::RBRACKET) {
		if (!(expr = parse_expr(false))) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::RBRACKET) {
		ERROR("Expected ']'");
		return nullptr;
	}
	next(); // Consume ']'
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	// range = ('[' ... ']' Type)
	auto range = beg_token.range.include(type->range());
	if (expr) {
		return new_node<AstArrayType>(type, expr, move(attrs), range);
	} else {
		return new_node<AstSliceType>(type, move(attrs), range);
	}
	BIRON_UNREACHABLE();
}

// FnType
//	::= 'fn' TupleType ('->' TupleType)?
AstFnType* Parser::parse_fn_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_FN) {
		ERROR("Expected 'fn'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'fn'
	auto args = parse_tuple_type(None{});
	if (!args) {
		return nullptr;
	}
	AstType* rets = nullptr;
	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		rets = parse_type();
		if (!rets) {
			return nullptr;
		}
		if (!rets->is_type<AstTupleType>()) {
			// Convert into a single element tuple
			Array<AstTupleType::Elem> elems{m_allocator};
			if (!elems.emplace_back(None{}, rets)) {
				return nullptr;
			}
			rets = new_node<AstTupleType>(move(elems), None{}, rets->range());
			if (!rets) {
				return nullptr;
			}
		}
	} else {
		Array<AstTupleType::Elem> elems{m_allocator};
		rets = new_node<AstTupleType>(move(elems), None{}, Range{0, 0});
		if (!rets) {
			return nullptr;
		}
	}
	auto range = beg_token.range.include(rets->range());
	return new_node<AstFnType>(args, static_cast<AstTupleType*>(rets), move(attrs), range);
}

// Stmt
//	::= BlockStmt
//	  | ReturnStmt
//	  | DeferStmt
//	  | IfStmt
//	  | LetStmt
//	  | ExprStmt
//	  | ForStmt
AstStmt* Parser::parse_stmt() noexcept {
	switch (peek().kind) {
	case Token::Kind::LBRACE:
		return parse_block_stmt();
	case Token::Kind::KW_RETURN:
		return parse_return_stmt();
	case Token::Kind::KW_DEFER:
		return parse_defer_stmt();
	case Token::Kind::KW_BREAK:
		return parse_break_stmt();
	case Token::Kind::KW_CONTINUE:
		return parse_continue_stmt();
	case Token::Kind::KW_IF:
		return parse_if_stmt();
	case Token::Kind::KW_LET:
		return parse_let_stmt(None{});
	case Token::Kind::KW_FOR:
		return parse_for_stmt();
	case Token::Kind::AT: {
		auto attrs = parse_attrs();
		if (!attrs) {
			return nullptr;
		}
		if (peek().kind != Token::Kind::KW_LET) {
			ERROR("Expected 'let' statement");
			return nullptr;
		}
		return parse_let_stmt(move(attrs));
	}
	default:
		return parse_expr_stmt(true);
	}
	BIRON_UNREACHABLE();
}

// BlockStmt
//	::= '{' Stmt* '}'
AstBlockStmt* Parser::parse_block_stmt() noexcept {
	Scope scope{m_allocator, m_scope};
	m_scope = &scope;
	if (peek().kind != Token::Kind::LBRACE) {
		ERROR("Expected '{'");
		return nullptr;
	}
	auto beg_token = next(); // Consume '{'
	Array<AstStmt*> stmts{m_allocator};
	while (peek().kind != Token::Kind::RBRACE) {
		AstStmt* stmt = parse_stmt();
		if (!stmt) {
			return nullptr;
		}
		if (!stmts.push_back(stmt)) {
			ERROR("Out of memory");
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::RBRACE) {
		ERROR("Expected '}'");
		return nullptr;
	}
	auto end_token = next(); // Consume '}'
	auto range = beg_token.range.include(end_token.range);
	auto node = new_node<AstBlockStmt>(move(stmts), range);
	if (!node) {
		return nullptr;
	}
	m_scope = m_scope->prev();
	return node;
}

// ReturnStmt
//	::= 'return' Expr? ';'
AstReturnStmt* Parser::parse_return_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_RETURN) {
		ERROR("Expected 'return'");
		return nullptr;
	}
	if (m_in_defer) {
		ERROR("Cannot use 'return' inside 'defer'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'return'
	AstExpr* expr = nullptr;
	if (peek().kind != Token::Kind::SEMI) {
		expr = parse_expr(false);
		if (!expr) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after return statement");
		return nullptr;
	}
	next(); // Consume ';'
	auto range = beg_token.range;
	if (expr) {
		range = range.include(expr->range());
	}
	return new_node<AstReturnStmt>(expr, range);
}

// DeferStmt
//	::= 'defer' Stmt
AstDeferStmt* Parser::parse_defer_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_DEFER) {
		ERROR("Expected 'defer'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'defer'
	m_in_defer = true;
	auto stmt = parse_stmt();
	if (!stmt) {
		return nullptr;
	}
	m_in_defer = false;
	return new_node<AstDeferStmt>(stmt, beg_token.range.include(stmt->range()));
}

// BreakStmt
//	::= 'break'
AstBreakStmt* Parser::parse_break_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_BREAK) {
		ERROR("Expected 'break'");
		return nullptr;
	}
	auto token = next(); // Consume 'break'
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after break statement");
		return nullptr;
	}
	next(); // Consume ';'
	return new_node<AstBreakStmt>(token.range);
}

// ContinueStmt
//	::= 'continue'
AstContinueStmt* Parser::parse_continue_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_CONTINUE) {
		ERROR("Expected 'continue'");
		return nullptr;
	}
	auto token = next(); // Consume 'continue'
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after continue statement");
		return nullptr;
	}
	next(); // Consume ';'
	return new_node<AstContinueStmt>(token.range);
}

// IfStmt
//	::= 'if' (LetStmt ';')? Expr BlockStmt ('else' (IfStmt | BlockStmt))?
AstIfStmt* Parser::parse_if_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_IF) {
		ERROR("Expected 'if'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'if'
	AstLetStmt* init = nullptr;
	// When we have a LET statement we introduce another scope. That is we compile
	//	if let ident ...; Expr {
	//		...
	//	} else {
	//		...
	//	}
	//
	// Into
	//	{
	//		let ident ...;
	//		if Expr {
	//			...
	//		} else {
	//			...
	//		}
	//	}
	//
	// Which makes the let declaration become available to the else and else if.
	Scope scope{m_allocator, m_scope};
	Bool scoped = false;
	if (peek().kind == Token::Kind::KW_LET) {
		m_scope = &scope;
		scoped = true;
		if (!(init = parse_let_stmt(None{}))) {
			return nullptr;
		}
	}
	// When inside an 'if' statement we do not parse any expression. We only parse
	auto expr = parse_expr(true);
	if (!expr) {
		return nullptr;
	}
	auto then = parse_block_stmt();
	if (!then) {
		return nullptr;
	}
	AstStmt* elif = nullptr;
	if (peek().kind == Token::Kind::KW_ELSE) {
		next(); // Consume 'else'
		if (peek().kind == Token::Kind::KW_IF) {
			elif = parse_if_stmt();
		} else {
			elif = parse_block_stmt();
		}
		if (!elif) {
			return nullptr;
		}
	}
	auto range = beg_token.range;
	if (init) range = range.include(init->range());
	range = range.include(expr->range());
	range = range.include(then->range());
	if (elif) range = range.include(elif->range());
	auto node = new_node<AstIfStmt>(init, expr, then, elif, range);
	if (!node) {
		return nullptr;
	}
	if (scoped) {
		m_scope = m_scope->prev();
	}
	return node;
}

// LetStmt
//	::= 'let' Ident ('=' Expr)? ';'
AstLetStmt* Parser::parse_let_stmt(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_LET) {
		ERROR("Expected 'let'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'let'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier for 'let' declaration");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range);
	AstExpr* init = nullptr;
	if (peek().kind != Token::Kind::EQ) {
		ERROR("Expected expression");
		return nullptr;
	}
	next(); // Consume '='
	init = parse_expr(false);
	if (!init) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after let statement");
		return nullptr;
	}
	next(); // Consume ';'
	auto range = beg_token.range.include(init->range());
	auto node = new_node<AstLetStmt>(name, init, move(attrs), range);
	if (!node) {
		return nullptr;
	}
	if (!m_scope->add_let(node)) {
		ERROR("Out of memory");
		return nullptr;
	}
	return node;
}

// Module
//	::= 'module' Ident
AstModule* Parser::parse_module() noexcept {
	if (peek().kind != Token::Kind::KW_MODULE) {
		ERROR("Expected 'module'");
		return nullptr;
	}
	auto module_token = next(); // Consume 'module'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier after 'module'");
		return nullptr;
	}
	auto ident_token = next(); // Consume Ident
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';'");
		return nullptr;
	}
	next(); // Consume ';'
	auto ident_string = m_lexer.string(ident_token.range);
	auto range = module_token.range.include(ident_token.range);
	return new_node<AstModule>(ident_string, range);
}

// Import
//	::= 'import' Ident
AstImport* Parser::parse_import() noexcept {
	if (peek().kind != Token::Kind::KW_MODULE) {
		ERROR("Expected 'import'");
		return nullptr;
	}
	auto import_token = next(); // Consume 'import'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier after 'import'");
		return nullptr;
	}
	auto ident_token = next(); // Consume Ident
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';'");
		return nullptr;
	}
	next(); // Consume ';'
	auto ident_string = m_lexer.string(ident_token.range);
	auto range = import_token.range.include(ident_token.range);
	return new_node<AstImport>(ident_string, range);
}

// ForStmt
//	::= 'for' BlockStmt
//	  | 'for' LetStmt? Expr BlockStmt
AstForStmt* Parser::parse_for_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_FOR) {
		ERROR("Expected 'for'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'for'
	AstLetStmt* let = nullptr;
	AstExpr* expr = nullptr;
	Scope scope{m_allocator, m_scope};
	if (peek().kind == Token::Kind::KW_LET) {
		m_scope = &scope;
		if (!(let = parse_let_stmt(None{}))) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::LBRACE) {
		if (!(expr = parse_expr(false))) {
			return nullptr;
		}
	}
	AstStmt* post = nullptr;
	if (peek().kind == Token::Kind::SEMI) {
		next(); // Consume ';'
		if (peek().kind == Token::Kind::LBRACE) {
			ERROR("Expected expression statement");
			return nullptr;
		}
		if (!(post = parse_expr_stmt(false))) {
			return nullptr;
		}
	}
	auto body = parse_block_stmt();
	if (!body) {
		return nullptr;
	}
	auto range = beg_token.range.include(body->range());
	auto node = new_node<AstForStmt>(let, expr, post, body, range);
	if (!node) {
		return nullptr;
	}
	m_scope = scope.prev();
	return node;
}

// ExprStmt
//	::= Expr ';'
//	  | Expr '=' Expr ';'
AstStmt* Parser::parse_expr_stmt(Bool semi) noexcept {
	auto expr = parse_expr(false);
	if (!expr) {
		return nullptr;
	}
	// TODO(dweiler): compound operators
	AstAssignStmt* assignment = nullptr; 
	auto token = peek();
	if (token.kind == Token::Kind::EQ
	 || token.kind == Token::Kind::PLUSEQ
	 || token.kind == Token::Kind::MINUSEQ
	 || token.kind == Token::Kind::STAREQ)
	{
		next(); // Consume '='
		auto value = parse_expr(false);
		if (!value) {
			return nullptr;
		}
		auto range = expr->range().include(value->range());
		using StoreOp = AstAssignStmt::StoreOp;
		StoreOp op;
		switch (token.kind) {
		/****/ case Token::Kind::EQ:      op = StoreOp::WR;
		break; case Token::Kind::PLUSEQ:  op = StoreOp::ADD;
		break; case Token::Kind::MINUSEQ: op = StoreOp::SUB;
		break; case Token::Kind::STAREQ:  op = StoreOp::MUL;
		break; default:
		break;
		}
		assignment = new_node<AstAssignStmt>(expr, value, op, range);
	}
	if (semi) {
		if (peek().kind != Token::Kind::SEMI) {
			ERROR("Expected ';' after expression");
			return nullptr;
		}
		next(); // Consume ';'
	}
	if (assignment) {
		return assignment;
	}
	return new_node<AstExprStmt>(expr, expr->range());
}

// Fn
//	::= 'fn' Ident TupleType ('->' Type)? BlockStmt
AstFn* Parser::parse_fn(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	Scope scope{m_allocator, m_scope};
	m_scope = &scope;
	if (peek().kind != Token::Kind::KW_FN) {
		ERROR("Expected 'fn'");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'fn'
	AstTupleType* selfs = nullptr;
	if (peek().kind == Token::Kind::LPAREN) {
		if (!(selfs = parse_tuple_type(None{}))) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected name for 'fn'");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range); // Consume Ident

	auto args = parse_tuple_type(None{});
	if (!args) {
		ERROR("Could not parse argument list");
		return nullptr;
	}

	AstType* rets = nullptr;
	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		rets = parse_type();
	} else {
		// When there are no return types we return the "empty tuple"
		rets = new_node<AstTupleType>(m_allocator, None{}, Range{0, 0});
	}
	if (!rets) {
		ERROR("Could not parse result list");
		return nullptr;
	}

	if (!rets->is_type<AstTupleType>()) {
		// The parser treats a single return type as-if it was a single element
		// tuple with that type.
		Array<AstTupleType::Elem> elems{m_allocator};
		if (!elems.emplace_back(None{}, rets)) {
			ERROR("Out of memory");
			return nullptr;
		}
		rets = new_node<AstTupleType>(move(elems), None{}, rets->range());
	}

	AstBlockStmt* body = parse_block_stmt();
	if (!body) {
		ERROR("Could not parse function body");
		return nullptr;
	}
	auto range = beg_token.range.include(body->range());
	auto node = new_node<AstFn>(name, selfs, args, static_cast<AstTupleType*>(rets), body, move(attrs), range);
	if (!node) {
		ERROR("Out of memory");
		return nullptr;
	}

	m_scope = m_scope->prev();

	if (!m_scope->add_fn(node)) {
		return nullptr;
	}

	return node;
}

// Typedef
//	::= 'type' Ident '=' Type ';'
AstTypedef* Parser::parse_typedef(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_TYPE) {
		ERROR("Expected type");
		return nullptr;
	}
	auto beg_token = next(); // Consume 'type'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	auto ident = next();
	auto name = m_lexer.string(ident.range);
	if (peek().kind != Token::Kind::EQ) {
		ERROR("Expected '='");
		return nullptr;
	}
	next(); // Consume '='
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';'");
		return nullptr;
	}
	auto end_token = next(); // Consume ';'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstTypedef>(name, type, move(attrs), range);
}

Maybe<Array<AstAttr*>> Parser::parse_attrs() noexcept {
	if (peek().kind != Token::Kind::AT) {
		ERROR("Expected @");
		return None{};
	}
	next(); // Consume '@'

	if (peek().kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return None{};
	}
	next(); // Consume '('

	Array<AstAttr*> attrs{m_allocator};
	while (peek().kind != Token::Kind::RPAREN) {
		if (peek().kind != Token::Kind::IDENT) {
			ERROR("Expected identifier");
			return None{};
		}
		auto token = next(); // Consume IDENT
		auto name = m_lexer.string(token.range);
		/****/ if (name == "section") {
		} else if (name == "align") {
		} else if (name == "used") {
		} else if (name == "inline") {
		} else if (name == "aliasable") {
		} else if (name == "redzone") {
		} else if (name == "alignstack") {
		} else if (name == "export") {
		} else {
			ERROR("Unknown attribute: '%S'", name);
			return None{};
		}
		auto args = parse_tuple_expr();
		if (!args || args->length() != 1) {
			return None{};
		}
		auto range = token.range.include(args->range());
		if (name == "section") {
			auto expr = args->at(0);
			auto value = expr->eval_value();
			if (!value || value->kind() != AstConst::Kind::STRING) {
				ERROR(expr->range(), "Expected constant string expression for section name");
				return None{};
			}
			using Kind = AstStringAttr::Kind;
			auto attr = new_node<AstStringAttr>(Kind::SECTION, value->as_string(), range);
			if (!attr || !attrs.push_back(attr)) {
				return None{};
			}
		} else if (name == "align" || name == "alignstack") {
			auto expr = args->at(0);
			auto value = expr->eval_value();
			if (!value || !value->is_integral()) {
				ERROR("Expected constant integer expression for alignment");
				return None{};
			}
			auto align = value->to<Uint64>();
			if (!align) {
				ERROR("Expected positive constant integer expression for alignment");
				return None{};
			}
			if (!is_pot(*align)) {
				ERROR("Alignment must be a power-of-two");
				return None{};
			}
			using Kind = AstIntAttr::Kind;
			auto kind = name == "align" ? Kind::ALIGN : Kind::ALIGNSTACK;
			auto attr = new_node<AstIntAttr>(kind, *align, range);
			if (!attr || !attrs.push_back(attr)) {
				return None{};
			}
		} else if (name == "used" || name == "inline" || name == "aliasable" || name == "redzone" || name == "export") {
			auto expr = args->at(0);
			auto value = expr->eval_value();
			if (!value || !value->is_bool()) {
				ERROR("Expected constant boolean expression for used attribute");
				return None{};
			}
			auto used = value->to<Bool32>();
			using Kind = AstBoolAttr::Kind;
			Kind kind;
			/**/ if (name == "used")      kind = Kind::USED;
			else if (name == "inline")    kind = Kind::INLINE;
			else if (name == "aliasable") kind = Kind::ALIASABLE;
			else if (name == "redzone")   kind = Kind::REDZONE;
			else if (name == "export")    kind = Kind::EXPORT;
			else return None{};
			auto attr = new_node<AstBoolAttr>(kind, *used, range);
			if (!attr || !attrs.push_back(attr)) {
				return None{};
			}
		}
		if (peek().kind != Token::Kind::COMMA) {
			break;
		}
		next(); // Consume ','
	}

	if (peek().kind != Token::Kind::RPAREN) {
		ERROR("Expected ')'");
		return None{};
	}
	next(); // Consume ')'

	return attrs;
}

Maybe<AstUnit> Parser::parse() noexcept {
	AstUnit unit{m_allocator};
	Scope scope{m_allocator, nullptr};
	m_scope = &scope;
	Maybe<Array<AstAttr*>> attrs;
	for (;;) switch (peek().kind) {
	case Token::Kind::AT:
		attrs = parse_attrs();
		if (!attrs) {
			return None{};
		}
		break;
	case Token::Kind::KW_FN:
		if (auto fn = parse_fn(move(attrs))) {
			if (!unit.add_fn(fn)) {
				ERROR("Out of memory");
				return None{};
			}
		}
		break;
	case Token::Kind::KW_TYPE:
		if (auto type = parse_typedef(move(attrs))) {
			if (!unit.add_typedef(type)) {
				ERROR("Out of memory");
				return None{};
			}
		}
		break;
	case Token::Kind::KW_LET:
		// We allow top-level constant declarations
		if (auto let = parse_let_stmt(move(attrs))) {
			if (!unit.add_let(let)) {
				ERROR("out of memory");
				return None{};
			}
		}
		break;
	case Token::Kind::KW_MODULE:
		if (auto module = parse_module()) {
			if (!unit.assign_module(module)) {
				ERROR("Duplicate 'module' in file");
				return None{};
			}
		}
		break;
	case Token::Kind::KW_IMPORT:
		if (auto import = parse_import()) {
			if (!unit.add_import(import)) {
				ERROR("Duplicate 'import' in file");
				return None{};
			}
		}
		break;
	case Token::Kind::END:
		return unit;
	default:
		ERROR("Unexpected token '%s' in top-level", peek().name());
		return None{};
	}
	BIRON_UNREACHABLE();
}

} // namespace Biron
