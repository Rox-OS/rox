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

// IndexExpr
//	::= '[' Expr ']'
AstExpr* Parser::parse_index_expr(AstExpr* operand) noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		return ERROR("Expected '['");
	}
	auto beg_token = peek();
	next(); // Consume '['
	auto index = parse_expr();
	if (!index) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::RBRACKET) {
		return ERROR("Expected ']'");
	}
	auto end_token = next(); // Consume ']'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstIndexExpr>(operand, index, range);
}

// CallExpr
//	::= Expr TupleExpr
AstExpr* Parser::parse_call_expr(AstExpr* operand) noexcept {
	auto args = parse_tuple_expr();
	if (!args) {
		return nullptr;
	}
	auto range = operand->range().include(args->range());
	Bool is_c = false;
	if (operand->is_expr<AstVarExpr>()) {
		is_c = static_cast<const AstVarExpr*>(operand)->name() == "printf";
	}
	return new_node<AstCallExpr>(operand, args, is_c, range);
}

AstExpr* Parser::parse_binop_rhs(int expr_prec, AstExpr* lhs) noexcept {
	using Op = AstBinExpr::Op;
	using LOp = AstLBinExpr::Op;
	for (;;) {
		auto peek_prec = peek().binary_prec();
		if (peek_prec < expr_prec) {
			return lhs;
		}
		auto token = next();
		auto kind = token.kind; // Eat Op
		AstExpr* rhs = nullptr;
		if (kind == Token::Kind::KW_AS || kind == Token::Kind::KW_IS) {
			rhs = parse_type_expr();
		} else {
			rhs = parse_unary_expr();
		}
		if (!rhs) {
			return nullptr;
		}
		// Handle less-tight binding for non-unary
		auto next_prec = peek().binary_prec();
		if (peek_prec < next_prec) {
			rhs = parse_binop_rhs(peek_prec + 1, rhs);
			if (!rhs) {
				return nullptr;
			}
		}
		// Map the token 'kind' to an AstBin::Operator
		auto range = token.range.include(lhs->range())
		                        .include(rhs->range());
		switch (kind) {
		/***/  case Token::Kind::KW_AS:  lhs = new_node<AstCastExpr>(lhs, rhs, range);
		break; case Token::Kind::KW_IS:  lhs = new_node<AstTestExpr>(lhs, rhs, range);
		break; case Token::Kind::KW_OF:  lhs = new_node<AstPropExpr>(lhs, rhs, range);
		break; case Token::Kind::STAR:   lhs = new_node<AstBinExpr>(Op::MUL,    lhs, rhs, range);
		break; case Token::Kind::FSLASH: lhs = new_node<AstBinExpr>(Op::DIV,    lhs, rhs, range);
		break; case Token::Kind::PLUS:   lhs = new_node<AstBinExpr>(Op::ADD,    lhs, rhs, range);
		break; case Token::Kind::MINUS:  lhs = new_node<AstBinExpr>(Op::SUB,    lhs, rhs, range);
		break; case Token::Kind::LSHIFT: lhs = new_node<AstBinExpr>(Op::LSHIFT, lhs, rhs, range);
		break; case Token::Kind::RSHIFT: lhs = new_node<AstBinExpr>(Op::RSHIFT, lhs, rhs, range);
		break; case Token::Kind::LT:     lhs = new_node<AstBinExpr>(Op::LT,     lhs, rhs, range);
		break; case Token::Kind::LTE:    lhs = new_node<AstBinExpr>(Op::LE,     lhs, rhs, range);
		break; case Token::Kind::MIN:    lhs = new_node<AstBinExpr>(Op::MIN,    lhs, rhs, range);
		break; case Token::Kind::GT:     lhs = new_node<AstBinExpr>(Op::GT,     lhs, rhs, range);
		break; case Token::Kind::GTE:    lhs = new_node<AstBinExpr>(Op::GE,     lhs, rhs, range);
		break; case Token::Kind::MAX:    lhs = new_node<AstBinExpr>(Op::MAX,    lhs, rhs, range);
		break; case Token::Kind::EQEQ:   lhs = new_node<AstBinExpr>(Op::EQ,     lhs, rhs, range);
		break; case Token::Kind::NEQ:    lhs = new_node<AstBinExpr>(Op::NE,     lhs, rhs, range);
		break; case Token::Kind::BAND:   lhs = new_node<AstBinExpr>(Op::BAND,   lhs, rhs, range);
		break; case Token::Kind::BOR:    lhs = new_node<AstBinExpr>(Op::BOR,    lhs, rhs, range);
		break; case Token::Kind::LAND:   lhs = new_node<AstLBinExpr>(LOp::LAND, lhs, rhs, range);
		break; case Token::Kind::LOR:    lhs = new_node<AstLBinExpr>(LOp::LOR,  lhs, rhs, range);
		break;
		default:
			return ERROR(token.range, "Unexpected token '%S' while parsing binary expression", token.name());
		}
	}
}

// PostfixExpr
//	::= PrimaryExpr '.' PrimaryExpr -- AstBinExpr (DOT)
//	  | PrimaryExpr '[' Expr ']'    -- AstIndexExpr
//	  | TypeExpr    '[' ']'         -- AstAggExpr
//	  | PrimaryExpr TupleExpr       -- AstCallExpr
//	  | TypeExpr    '{' Expr* '}'   -- AstAggExpr
AstExpr* Parser::parse_postfix_expr() noexcept {
	auto operand = parse_primary_expr();
	if (!operand) {
		return nullptr;
	}
	// We can have a single '!' when referencing an effect.
	if (peek().kind == Token::Kind::NOT) {
		auto token = next(); // Consume '!'
		operand = new_node<AstEffExpr>(operand, operand->range().include(token.range));
	}
	for (;;) {
		switch (peek().kind) {
		case Token::Kind::DOT:
			{
				next(); // Consume '.'
				auto expr = parse_primary_expr();
				if (!expr) {
					return nullptr;
				}
				auto range = operand->range().include(expr->range());
				operand = new_node<AstAccessExpr>(operand, expr, range);
			}
			break;
		case Token::Kind::LBRACKET:
			if (!(operand = parse_index_expr(operand))) {
				return nullptr;
			}
			break;
		case Token::Kind::LPAREN:
			if (!(operand = parse_call_expr(operand))) {
				return nullptr;
			}
			break;
		case Token::Kind::LBRACE:
			if (operand->is_expr<AstTypeExpr>()) {
				if (!(operand = parse_agg_expr(operand))) {
					return nullptr;
				}
			} else {
				return operand;
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
AstExpr* Parser::parse_unary_expr() noexcept {
	using Op = AstUnaryExpr::Op;
	AstExpr* operand = nullptr;
	auto token = peek();
	switch (token.kind) {
	case Token::Kind::NOT:
		next(); // Consume '!'
		if (!(operand = parse_unary_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::NOT, operand, token.range.include(operand->range()));
	case Token::Kind::MINUS:
		next(); // Consume '-'
		if (!(operand = parse_unary_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::NEG, operand, token.range.include(operand->range()));
	case Token::Kind::PLUS:
		next(); // Consume '+'
		return parse_unary_expr();
	case Token::Kind::STAR:
		next(); // Consume '*'
		if (!(operand = parse_unary_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::DEREF, operand, token.range.include(operand->range()));
	case Token::Kind::BAND:
		next(); // Consume '&'
		if (!(operand = parse_unary_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(Op::ADDROF, operand, token.range.include(operand->range()));
	case Token::Kind::ELLIPSIS:
		next();
		if (!(operand = parse_unary_expr())) {
			return nullptr;
		}
		return new_node<AstExplodeExpr>(operand, token.range.include(operand->range()));
	default:
		return parse_postfix_expr();
	}
	BIRON_UNREACHABLE();
}

// Expr
//	::= PrimaryExpr BinOpRHS
AstExpr* Parser::parse_expr() noexcept {
	auto lhs = parse_unary_expr();
	if (!lhs) {
		return nullptr;
	}
	return parse_binop_rhs(0, lhs);
}

// PrimaryExpr
//	::= VarExpr
//	  | BoolExpr
//	  | IntExpr
//	  | StrExpr
//	  | ChrExpr
//	  | TupleExpr
//	  | TypeExpr (* and @)
AstExpr* Parser::parse_primary_expr() noexcept {
	switch (peek().kind) {
	case Token::Kind::DOT:
		return parse_selector_expr();
	case Token::Kind::KW_TYPE:
		[[fallthrough]];
	case Token::Kind::IDENT:
		return parse_var_expr();
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
	case Token::Kind::KW_NEW:
		next(); // Consume 'new'
		return parse_type_expr();
	case Token::Kind::LBRACE:
		// Aggregate initializer without any type specified
		return parse_agg_expr(nullptr);
	default:
		break;
	}
	return ERROR("Unknown token '%S' in primary expression", peek().name());
}

AstExpr* Parser::parse_agg_expr(AstExpr* type_expr) noexcept {
	AstType* type = nullptr;
	if (type_expr) {
		if (auto expr = type_expr->to_expr<AstTypeExpr>()) {
			type = expr->type();
		} else {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::LBRACE) {
		return type_expr;
	}
	Array<AstExpr*> exprs{m_arena};
	auto beg_token = next(); // Skip '{'
	auto range = beg_token.range;
	while (peek().kind != Token::Kind::RBRACE) {
		auto expr = parse_expr();
		if (!expr) {
			return nullptr;
		}
		if (!exprs.push_back(expr)) {
			return oom();
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::RBRACE) {
		return ERROR("Expected '}'");
	}
	auto end_token = next(); // Skip '}'

	range = range.include(end_token.range);

	if (type) {
		range = range.include(type->range());
	}

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

// VarExpr
//	::= Ident
AstExpr* Parser::parse_var_expr() noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier");
	}
	auto token = next(); // Consume ident
	auto name = m_lexer.string(token.range);
	return new_node<AstVarExpr>(name, token.range);
}

// SelectorExpr
//	::= '.' Ident
AstExpr* Parser::parse_selector_expr() noexcept {
	if (peek().kind != Token::Kind::DOT) {
		return ERROR("Expected '.'");
	}
	auto beg_token = next(); // Consume '.'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier");
	}
	auto name_token = next(); // Consume Ident
	auto name = m_lexer.string(name_token.range);
	auto range = beg_token.range.include(name_token.range);
	return new_node<AstSelectorExpr>(name, range);
}

// IntExpr
//	::= '0x' (HexDigit DigitSep?)+
//	  | '0b' (BinDigit DigitSep?)+
//	  | (DecDigit DigitSep?)+
// DigitSep
//	::= "'"
AstIntExpr* Parser::parse_int_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_INT) {
		return ERROR("Expected int literal");
	}

	auto token = next();

	// Filter out digit separator '
	StringBuilder builder{m_arena};
	auto lit = m_lexer.string(token.range);
	for (Ulen l = lit.length(), i = 0; i < l; i++) {
		if (lit[i] != '\'') {
			builder.append(lit[i]);
		} else if (i == l - 1 || lit[i + 1] == '_') {
			// The integer literal should not end with trailing ' or '_T
			auto skip = token.range;
			skip.offset += i;
			skip.length -= i;
			return ERROR(skip, "Unexpected trailing digits separator in integer literal");
		}
	}
	builder.append('\0');
	if (!builder.valid()) {
		return oom();
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
			return ERROR("Integer literal '%S' too large", match.name);
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
		return ERROR("Expected character literal");
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
		return ERROR("Expected float literal");
	}

	auto token = next();

	// Filter out digit separator '
	StringBuilder builder{m_arena};
	auto lit = m_lexer.string(token.range);
	for (Ulen l = lit.length(), i = 0; i < l; i++) {
		if (lit[i] != '\'') {
			builder.append(lit[i]);
		} else if (i == l - 1 || lit[i + 1] == '_') {
			// The floating-point literal should not end with trailing ' or '_T
			auto skip = token.range;
			skip.offset += i;
			skip.length -= i;
			return ERROR(skip, "Unexpected trailing digits separator in floating-point literal");
		}
	}
	builder.append('\0');
	if (!builder.valid()) {
		return oom();
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
		return ERROR("Expected string literal");
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
		return ERROR("Expected 'true' or 'false'");
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
		return ERROR("Expected '('");
	}
	next(); // Consume '('
	Array<AstExpr*> exprs{m_arena};
	while (peek().kind != Token::Kind::RPAREN) {
		auto expr = parse_expr();
		if (!expr) {
			return nullptr;
		}
		if (!exprs.push_back(expr)) {
			return oom();
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
			if (peek().kind == Token::Kind::RPAREN) {
				return ERROR("Expected expression");
			}
		} else {
			break;
		}
	}
	auto end_token = peek();
	if (end_token.kind != Token::Kind::RPAREN) {
		return ERROR("Expected ')' to terminate tuple expression");
	}
	next(); // Consume ')'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstTupleExpr>(move(exprs), range);
}

// Type
//	::= IdentType
//	  | TupleType
//	  | PtrType
//	  | AtomType
//	  | ArrayType
//	  | SliceType
//	  | FnType
//	  | VarArgsType
//	  | UnionType
// UnionType
//	::= Type '|' Type ('|' Type)*
AstType* Parser::parse_type() noexcept {
	Array<AstType*> types{m_arena};
	Array<AstAttr*> attrs{m_arena};
	// | separates types for union
	for (;;) {
		AstType* type = nullptr;
		switch (peek().kind) {
		case Token::Kind::IDENT:
			type = parse_ident_type(move(attrs));
			break;
		case Token::Kind::LBRACE:
			type = parse_tuple_type(move(attrs));
			break;
		case Token::Kind::LPAREN:
			type = parse_group_type();
			break;
		case Token::Kind::STAR:
			type = parse_ptr_type(move(attrs));
			break;
		case Token::Kind::AT:
			next(); // Consume '@'
			if (peek().kind == Token::Kind::LPAREN) {
				if (auto at = parse_attrs()) {
					attrs = move(*at);
				} else {
					return nullptr;
				}
				continue;
			} else {
				type = parse_atom_type(move(attrs));
			}
			break;
		case Token::Kind::LBRACKET:
			type = parse_bracket_type(move(attrs));
			break;
		case Token::Kind::KW_FN:
			type = parse_fn_type(move(attrs));
			break;
		case Token::Kind::ELLIPSIS:
			type = parse_varargs_type(move(attrs));
			break;
		default:
			return ERROR("Unexpected token '%S' while parsing type", peek().name());
		}
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			return oom();
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
AstIdentType* Parser::parse_ident_type(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier");
	}
	auto token = next(); // consume Ident
	auto name = m_lexer.string(token.range);
	return new_node<AstIdentType>(name, move(attrs), token.range);
}

// TupleType
//	::= '{' '}'
//	  | '{' TupleElem (',' TupleElem)* '}'
// TupleElem
//	::= (Ident ':')? Type
AstTupleType* Parser::parse_tuple_type(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::LBRACE) {
		return ERROR("Expected '{'");
	}
	auto beg_token = next(); // Consume '('
	Array<AstTupleType::Elem> elems{m_arena};
	while (peek().kind != Token::Kind::RBRACE) {
		auto type = parse_type();
		if (!type) {
			return nullptr;
		}
		if (peek().kind == Token::Kind::COLON) {
			if (auto ident = type->to_type<AstIdentType>()) {
				next(); // Consume ':'
				auto type = parse_type();
				if (!type) {
					return nullptr;
				}
				if (!elems.emplace_back(ident->name(), type)) {
					return oom();
				}
			} else {
				return error(type->range(), "Expected identifier");
			}
		} else if (!elems.emplace_back(None{}, type)) {
			return oom();
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::RBRACE) {
		return ERROR("Expected '}' to terminate tuple type");
	}
	auto end_token = next(); // Consume '}'
	return new_node<AstTupleType>(move(elems),
	                              attrs ? move(*attrs) : AttrArray{ m_arena },
	                              beg_token.range.include(end_token.range));
}

// ArgsType
//	::= '(' ')'
//	  | '(' ArgsElem (',' ArgsElem)* ')'
// ArgsElem
//	::= (Ident ':')? Type
AstArgsType* Parser::parse_args_type() noexcept {
	if (peek().kind != Token::Kind::LPAREN) {
		return ERROR("Expected '('");
	}
	auto beg_token = next(); // Consume '('
	Array<AstArgsType::Elem> elems{m_arena};
	while (peek().kind != Token::Kind::RPAREN) {
		auto type = parse_type();
		if (!type) {
			return nullptr;
		}
		if (peek().kind == Token::Kind::COLON) {
			if (auto ident = type->to_type<AstIdentType>()) {
				next(); // Consume ':'
				auto type = parse_type();
				if (!type) {
					return nullptr;
				}
				if (!elems.emplace_back(ident->name(), type)) {
					return oom();
				}
			} else {
				return error(type->range(), "Expected identifier");
			}
		} else if (!elems.emplace_back(None{}, type)) {
			return oom();
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::RPAREN) {
		return ERROR("Expected ')' to terminate arguments");
	}
	auto end_token = next(); // Consume ')'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstArgsType>(move(elems), range);
}

// GroupType
//	::= '(' Type+ ')'
AstGroupType* Parser::parse_group_type() noexcept {
	if (peek().kind != Token::Kind::LPAREN) {
		return ERROR("Expected '('");
	}
	auto beg_token = next(); // Consume '('
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::RPAREN) {
		return ERROR("Expected ')'");
	}
	auto end_token = next(); // Consume ')'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstGroupType>(type, range);
}

// VarArgsType
//	::= '...'
AstVarArgsType* Parser::parse_varargs_type(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::ELLIPSIS) {
		return ERROR("Expected '...'");
	}
	auto token = next(); // Consume '...'
	return new_node<AstVarArgsType>(move(attrs), token.range);
}

// PtrType
//	::= '*' Type
AstPtrType* Parser::parse_ptr_type(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::STAR) {
		return ERROR("Expected '*'");
	}
	auto token = next(); // Consume '*'
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstPtrType>(type, move(attrs), token.range.include(type->range()));
}

// AtomType
//	::= '@' Type
AstAtomType* Parser::parse_atom_type(Array<AstAttr*>&& attrs) noexcept {
	// The '@' is consumed by the caller.
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstAtomType>(type, move(attrs), type->range());
}

// BracketType
//	::= EnumType
//	  | ArrayType
//	  | SliceType
// ArrayType
//	::= '[' (Expr | '?') ']' Type
// SliceType
//	::= '[' ']' Type
// EnumType
//	::= '[' Enum (',' Enum)* ']'
// Enum
//	::= '.' Ident ('=' Expr)?
AstType* Parser::parse_bracket_type(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		return ERROR("Expected '['");
	}
	auto beg_token = next(); // Consume '['
	Array<AstEnumType::Enumerator> enums{m_arena};
	AstExpr* expr = nullptr;
	if (peek().kind == Token::Kind::DOT) {
		while (peek().kind != Token::Kind::RBRACKET) {
			if (peek().kind != Token::Kind::DOT) {
				return ERROR("Expected '.' prefix in enumerator");
			}
			next(); // Consume '.'
			if (peek().kind != Token::Kind::IDENT) {
				return ERROR("Expected identifier");
			}
			auto name_token = next(); // Consume Ident
			AstExpr* init = nullptr;
			if (peek().kind == Token::Kind::EQ) {
				next(); // Consume '='
				init = parse_expr();
				if (!init) {
					return nullptr;
				}
			}
			auto name = m_lexer.string(name_token.range);
			if (!enums.emplace_back(name, init)) {
				return oom();
			}
			if (peek().kind == Token::Kind::COMMA) {
				next(); // Consume ','
			} else {
				break;
			}
		}
	} else if (peek().kind == Token::Kind::QUESTION) {
		auto token = next(); // Consume '?'
		expr = new_node<AstInferSizeExpr>(token.range);
	} else {
		expr = parse_expr();
	}
	if (enums.empty() && !expr) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::RBRACKET) {
		return ERROR("Expected ']'");
	}
	auto end_token = next(); // Consume ']'
	auto range = beg_token.range.include(end_token.range);
	if (enums.empty()) {
		auto type = parse_type();
		if (!type) {
			return nullptr;
		}
		range = range.include(type->range());
		if (expr) {
			return new_node<AstArrayType>(type, expr, move(attrs), range);
		} else {
			return new_node<AstSliceType>(type, move(attrs), range);
		}
	}
	return new_node<AstEnumType>(move(enums), move(attrs), range);
}

// FnType
//	::= 'fn' ArgsType ArgsType? Effects? ('->' Type)? BlockStmt
// Effects
//	::= '<' Ident (',' Ident)* '>'
AstFnType* Parser::parse_fn_type(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_FN) {
		return ERROR("Expected 'fn'");
	}

	auto beg_token = next(); // Consume 'fn'

	// This might be the objs or args list.
	AstArgsType* tuple = parse_args_type();
	if (!tuple) {
		return nullptr;
	}
	
	AstArgsType* objs = nullptr;
	AstArgsType* args = nullptr;
	if (peek().kind == Token::Kind::LPAREN) {
		objs = tuple;
		args = parse_args_type();
		if (!args) {
			return nullptr;
		}
	} else {
		// When there are no objs we use an empty args.
		objs = new_node<AstArgsType>(m_arena, Range{0, 0});
		if (!objs) {
			return nullptr;
		}
		args = tuple;
	}

	Array<AstIdentType*> effects{m_arena};
	if (peek().kind == Token::Kind::LT) {
		next(); // Consume '<'
		while (peek().kind != Token::Kind::GT) {
			auto type = parse_ident_type({m_arena});
			if (!type) {
				return nullptr;
			}
			if (!effects.push_back(type)) {
				return oom();
			}
			if (peek().kind == Token::Kind::COMMA) {
				next(); // Consume ','
			} else {
				break;
			}
		}
		if (peek().kind != Token::Kind::GT) {
			return ERROR("Expected '>'");
		}
		next(); // Consume '>'
	}

	AstType* ret = nullptr;
	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		ret = parse_type();
		if (!ret) {
			return nullptr;
		}
	} else {
		// When there are no return types we return the "empty tuple"
		ret = new_node<AstTupleType>(m_arena, m_arena, Range{0, 0});
		if (!ret) {
			return oom();
		}
	}

	auto range = beg_token.range.include(ret->range());

	return new_node<AstFnType>(objs, args, move(effects), ret, move(attrs), range);
}

// Stmt
//	::= BlockStmt
//	  | ReturnStmt
//	  | DeferStmt
//	  | IfStmt
//	  | LetStmt
//	  | UsingStmt
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
	case Token::Kind::KW_USING:
		return parse_using_stmt();
	case Token::Kind::KW_FOR:
		return parse_for_stmt();
	case Token::Kind::AT:
		{
			next(); // Consume '@'
			auto attrs = parse_attrs();
			if (!attrs) {
				return nullptr;
			}
			if (peek().kind != Token::Kind::KW_LET) {
				return ERROR("Expected 'let' statement");
			}
			return parse_let_stmt(move(*attrs));
		}
		break;
	default:
		return parse_expr_stmt(true);
	}
	BIRON_UNREACHABLE();
}

// BlockStmt
//	::= '{' Stmt* '}'
AstBlockStmt* Parser::parse_block_stmt() noexcept {
	if (peek().kind != Token::Kind::LBRACE) {
		return ERROR("Expected '{'");
	}
	auto beg_token = next(); // Consume '{'
	Array<AstStmt*> stmts{m_arena};
	while (peek().kind != Token::Kind::RBRACE) {
		AstStmt* stmt = parse_stmt();
		if (!stmt) {
			return nullptr;
		}
		if (!stmts.push_back(stmt)) {
			return oom();
		}
	}
	if (peek().kind != Token::Kind::RBRACE) {
		return ERROR("Expected '}'");
	}
	auto end_token = next(); // Consume '}'
	auto range = beg_token.range.include(end_token.range);
	auto node = new_node<AstBlockStmt>(move(stmts), range);
	if (!node) {
		return nullptr;
	}
	return node;
}

// ReturnStmt
//	::= 'return' Expr? ';'
AstReturnStmt* Parser::parse_return_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_RETURN) {
		return ERROR("Expected 'return'");
	}
	if (m_in_defer) {
		return ERROR("Cannot use 'return' inside 'defer'");
	}
	auto beg_token = next(); // Consume 'return'
	AstExpr* expr = nullptr;
	if (peek().kind != Token::Kind::SEMI) {
		expr = parse_expr();
		if (!expr) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';' after return statement");
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
		return ERROR("Expected 'defer'");
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
//	::= 'break' ';'
AstBreakStmt* Parser::parse_break_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_BREAK) {
		return ERROR("Expected 'break'");
	}
	auto token = next(); // Consume 'break'
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';' after break statement");
	}
	next(); // Consume ';'
	return new_node<AstBreakStmt>(token.range);
}

// ContinueStmt
//	::= 'continue' ';'
AstContinueStmt* Parser::parse_continue_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_CONTINUE) {
		return ERROR("Expected 'continue'");
	}
	auto token = next(); // Consume 'continue'
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';' after continue statement");
	}
	next(); // Consume ';'
	return new_node<AstContinueStmt>(token.range);
}

// IfStmt
//	::= 'if' LetStmt? Expr BlockStmt ('else' (IfStmt | BlockStmt))?
AstIfStmt* Parser::parse_if_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_IF) {
		return ERROR("Expected 'if'");
	}
	auto beg_token = next(); // Consume 'if'
	AstLetStmt* init = nullptr;
	if (peek().kind == Token::Kind::KW_LET) {
		init = parse_let_stmt(None{});
		if (!init) {
			return nullptr;
		}
	}
	auto expr = parse_expr();
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
	return node;
}

// LetStmt
//	::= 'let' Ident '=' Expr ';'
AstLetStmt* Parser::parse_let_stmt(Maybe<Array<AstAttr*>>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_LET) {
		return ERROR("Expected 'let'");
	}
	auto beg_token = next(); // Consume 'let'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier after 'let'");
	}
	auto token = next(); // Consume Ident
	auto name = m_lexer.string(token.range);
	if (peek().kind != Token::Kind::EQ) {
		return ERROR("Expected expression");
	}
	next(); // Consume '='
	auto init = parse_expr();
	if (!init) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';' after 'let' statement");
	}
	next(); // Consume ';'
	auto range = beg_token.range.include(init->range());
	return new_node<AstLetStmt>(name,
	                            init,
	                            attrs ? move(*attrs) : AttrArray{m_arena},
	                            range);
}

// UsingStmt
//	::= 'using' Ident '=' Expr ';'
AstUsingStmt* Parser::parse_using_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_USING) {
		return ERROR("Expected 'using'");
	}
	auto using_token = next(); // Consume 'using'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier after 'using'");
	}
	auto token = next(); // Consume Ident
	auto name = m_lexer.string(token.range);
	if (peek().kind != Token::Kind::EQ) {
		return ERROR("Expected expression");
	}
	next(); // Consume '='
	auto init = parse_expr();
	if (!init) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';' after 'using' statement");
	}
	next(); // Consume ';'
	auto range = using_token.range.include(init->range());
	return new_node<AstUsingStmt>(name, init, range);
}

// Module
//	::= 'module' Ident ';'
AstModule* Parser::parse_module() noexcept {
	if (peek().kind != Token::Kind::KW_MODULE) {
		return ERROR("Expected 'module'");
	}
	auto module_token = next(); // Consume 'module'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier after 'module'");
	}
	auto ident_token = next(); // Consume Ident
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';'");
	}
	next(); // Consume ';'
	auto ident_string = m_lexer.string(ident_token.range);
	auto range = module_token.range.include(ident_token.range);
	return new_node<AstModule>(ident_string, range);
}

// Import
//	::= 'import' Ident ';'
AstImport* Parser::parse_import() noexcept {
	if (peek().kind != Token::Kind::KW_IMPORT) {
		return ERROR("Expected 'import'");
	}
	auto import_token = next(); // Consume 'import'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier after 'import'");
	}
	auto ident_token = next(); // Consume Ident
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';'");
	}
	next(); // Consume ';'
	auto ident_string = m_lexer.string(ident_token.range);
	auto range = import_token.range.include(ident_token.range);
	return new_node<AstImport>(ident_string, range);
}

// Effect
//	::= 'effect' Ident '=' Type ';'
AstEffect* Parser::parse_effect() noexcept {
	if (peek().kind != Token::Kind::KW_EFFECT) {
		return ERROR("Expected 'effect'");
	}
	auto effect_token = next(); // Consume 'effect'
	auto range = effect_token.range;
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier after 'effect'");
	}
	auto ident_token = next(); // Consume ident
	range = range.include(ident_token.range);
	if (peek().kind != Token::Kind::EQ) {
		return ERROR("Expected '='");
	}
	next(); // Consume '='
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	range = range.include(type->range());
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';'");
	}
	next(); // Consume ';'
	auto ident_string = m_lexer.string(ident_token.range);
	return new_node<AstEffect>(ident_string, type, range);
}

// ForStmt
//	::= 'for' BlockStmt
//	  | 'for' LetStmt? Expr BlockStmt ('else' BlockStmt)?
AstForStmt* Parser::parse_for_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_FOR) {
		return ERROR("Expected 'for'");
	}
	auto beg_token = next(); // Consume 'for'
	AstLetStmt* let = nullptr;
	AstExpr* expr = nullptr;
	if (peek().kind == Token::Kind::KW_LET) {
		if (!(let = parse_let_stmt(None{}))) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::LBRACE) {
		if (!(expr = parse_expr())) {
			return nullptr;
		}
	}
	AstStmt* post = nullptr;
	if (peek().kind == Token::Kind::SEMI) {
		next(); // Consume ';'
		if (peek().kind == Token::Kind::LBRACE) {
			return ERROR("Expected expression statement");
		}
		if (!(post = parse_expr_stmt(false))) {
			return nullptr;
		}
	}
	auto body = parse_block_stmt();
	if (!body) {
		return nullptr;
	}
	AstBlockStmt* elze = nullptr;
	if (peek().kind == Token::Kind::KW_ELSE) {
		next(); // Consume 'else'
		elze = parse_block_stmt();
		if (!elze) {
			return nullptr;
		}
	}
	auto range = beg_token.range.include(body->range());
	auto node = new_node<AstForStmt>(let, expr, post, body, elze, range);
	if (!node) {
		return nullptr;
	}
	return node;
}

// ExprStmt
//	::= Expr ';'
//	  | Expr '=' Expr ';'
AstStmt* Parser::parse_expr_stmt(Bool semi) noexcept {
	auto expr = parse_expr();
	if (!expr) {
		return nullptr;
	}
	// TODO(dweiler): compound operators
	AstAssignStmt* assignment = nullptr; 
	auto token = peek();
	if (token.kind == Token::Kind::EQ
	 || token.kind == Token::Kind::PLUSEQ
	 || token.kind == Token::Kind::MINUSEQ
	 || token.kind == Token::Kind::STAREQ
	 || token.kind == Token::Kind::FSLASHEQ)
	{
		next(); // Consume '='
		auto value = parse_expr();
		if (!value) {
			return nullptr;
		}
		auto range = expr->range().include(value->range());
		using StoreOp = AstAssignStmt::StoreOp;
		StoreOp op;
		switch (token.kind) {
		/****/ case Token::Kind::EQ:       op = StoreOp::WR;
		break; case Token::Kind::PLUSEQ:   op = StoreOp::ADD;
		break; case Token::Kind::MINUSEQ:  op = StoreOp::SUB;
		break; case Token::Kind::STAREQ:   op = StoreOp::MUL;
		break; case Token::Kind::FSLASHEQ: op = StoreOp::DIV;
		break; default: return nullptr;
		break;
		}
		assignment = new_node<AstAssignStmt>(expr, value, op, range);
	}
	if (semi) {
		if (peek().kind != Token::Kind::SEMI) {
			return ERROR("Expected ';' after expression");
		}
		next(); // Consume ';'
	}
	if (assignment) {
		return assignment;
	}
	return new_node<AstExprStmt>(expr, expr->range());
}

// Fn
//	::= 'fn' ArgsType? Ident ArgsType ('<' Ident (',' Ident)* '>')? ('->' Type)? BlockStmt
AstFn* Parser::parse_fn(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_FN) {
		return ERROR("Expected 'fn'");
	}
	auto beg_token = next(); // Consume 'fn'
	AstArgsType* objs = nullptr;
	if (peek().kind == Token::Kind::LPAREN) {
		objs = parse_args_type();
	} else {
		// When there are no objs we use an "empty" argument list type.
		objs = new_node<AstArgsType>(m_arena, Range{0, 0});
	}
	if (!objs) {
		return ERROR("Could not parse recievers");
	}
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected name for 'fn'");
	}
	auto token = next();
	auto name = m_lexer.string(token.range); // Consume Ident

	auto args = parse_args_type();
	if (!args) {
		return nullptr;
	}

	AstType* ret = nullptr;
	Array<AstIdentType*> effects{m_arena};
	if (peek().kind == Token::Kind::LT) {
		next(); // Consume '<'
		while (peek().kind != Token::Kind::GT) {
			auto type = parse_ident_type({m_arena});
			if (!type) {
				return nullptr;
			}
			if (!effects.push_back(type)) {
				return oom();
			}
			if (peek().kind == Token::Kind::COMMA) {
				next(); // Consume ','
			} else {
				break;
			}
		}
		if (peek().kind != Token::Kind::GT) {
			return ERROR("Expected '>'");
		}
		next(); // Consume '>'
	}

	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		ret = parse_type();
	} else {
		// When there are no return types we return the "empty tuple"
		ret = new_node<AstTupleType>(m_arena, m_arena, Range{0, 0});
	}
	if (!ret) {
		return ERROR("Could not parse returns");
	}

	AstBlockStmt* body = parse_block_stmt();
	if (!body) {
		return nullptr;
	}
	auto range = beg_token.range.include(body->range());
	auto node = new_node<AstFn>(name, objs, args, move(effects), ret, body, move(attrs), range);
	if (!node) {
		return oom();
	}

	return node;
}

// Typedef
//	::= 'type' Ident '=' Type ';'
AstTypedef* Parser::parse_typedef(Array<AstAttr*>&& attrs) noexcept {
	if (peek().kind != Token::Kind::KW_TYPE) {
		return ERROR("Expected type");
	}
	auto beg_token = next(); // Consume 'type'
	if (peek().kind != Token::Kind::IDENT) {
		return ERROR("Expected identifier");
	}
	auto ident = next();
	auto name = m_lexer.string(ident.range);
	if (peek().kind != Token::Kind::EQ) {
		return ERROR("Expected '='");
	}
	next(); // Consume '='
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		return ERROR("Expected ';'");
	}
	auto end_token = next(); // Consume ';'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstTypedef>(name, type, move(attrs), range);
}

// AttrList
//	::= '@' '(' Attr (',' Attr)* ')'
// Attr
//	::= Ident TupleExpr
Maybe<Array<AstAttr*>> Parser::parse_attrs() noexcept {
	// The '@' is consumed by the caller
	if (peek().kind != Token::Kind::LPAREN) {
		return ERROR("Expected '('");
	}
	next(); // Consume '('

	Array<AstAttr*> attrs{m_arena};
	while (peek().kind != Token::Kind::RPAREN) {
		if (peek().kind != Token::Kind::IDENT) {
			return ERROR("Expected identifier");
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
			return ERROR("Unknown attribute: '%S'", name);
		}
		auto args = parse_tuple_expr();
		if (!args || args->length() != 1) {
			return None{};
		}
		auto range = token.range.include(args->range());
		auto attr = new_node<AstAttr>(name, args->at(0), range);
		if (!attr) {
			return None{};
		}
		if (!attrs.push_back(attr)) {
			return oom();
		}
		if (peek().kind != Token::Kind::COMMA) {
			break;
		}
		next(); // Consume ','
	}

	if (peek().kind != Token::Kind::RPAREN) {
		return ERROR("Expected ')'");
	}
	next(); // Consume ')'

	return attrs;
}

Maybe<AstUnit> Parser::parse() noexcept {
	AstUnit unit{m_arena};
	Array<AstAttr*> attrs{m_arena};
	for (;;) switch (peek().kind) {
	case Token::Kind::AT:
		next(); // Consume '@'
		if (auto at = parse_attrs()) {
			attrs = move(*at);
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_FN:
		if (auto fn = parse_fn(move(attrs))) {
			if (!unit.add_fn(fn)) {
				return oom();
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_TYPE:
		if (auto type = parse_typedef(move(attrs))) {
			if (!unit.add_typedef(type)) {
				return oom();
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_LET:
		// We allow top-level constant declarations
		if (auto let = parse_let_stmt(move(attrs))) {
			if (!unit.add_let(let)) {
				return oom();
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_MODULE:
		if (auto module = parse_module()) {
			if (!unit.assign_module(module)) {
				return ERROR("Duplicate 'module' in file");
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_IMPORT:
		if (auto import = parse_import()) {
			if (!unit.add_import(import)) {
				return ERROR("Duplicate 'import' in file");
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::KW_EFFECT:
		if (auto effect = parse_effect()) {
			if (!unit.add_effect(effect)) {
				return oom();
			}
		} else {
			return None{};
		}
		break;
	case Token::Kind::END:
		return unit;
	default:
		return ERROR("Unexpected token '%S' while parsing top-level", peek().name());
	}
	BIRON_UNREACHABLE();
}

} // namespace Biron
