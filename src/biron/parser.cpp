#include <biron/parser.h>
#include <biron/ast.h>
#include <biron/util/unreachable.inl>

#include <string.h> // memcpy
#include <stdlib.h> // strtoul
#include <stdio.h>  // fprintf

#define ERROR(...) \
	error(__VA_ARGS__)

namespace Biron {

Bool Scope::find(StringView name) const noexcept {
	// Just assume printf exists for debugging purposes.
	if (name == "printf") {
		return true;
	}
	for (Ulen l = lets.length(), i = 0; i < l; i++) {
		if (lets[i]->name == name) {
			return true;
		}
	}
	// Check optional function arguments.
	if (args) {
		for (Ulen l = args->elems.length(), i = 0; i < l; i++) {
			auto& elem = args->elems[i];
			if (elem.name && *elem.name == name) {
				return true;
			}
		}
	}
	return false;
}

Bool Parser::has_symbol(StringView name) const noexcept {
	for (auto scope = m_scope; scope; scope = scope->prev) {
		if (scope->find(name)) {
			return true;
		}
	}
	return false;
}

Parser::~Parser() noexcept {
	for (Ulen l = m_nodes.length(), i = 0; i < l; i++) {
		m_nodes[i]->~AstNode();
		m_allocator.deallocate(m_nodes[i], 0);
	}
}

void Parser::diagnostic(Token where, const char *message) {
	// Do not report the same error more than once
	static Range last_range{0, 0};
	Range range = where.range;
	if (range == last_range) return;
	last_range = range;

	// Work out the column and line from the token offset.
	Ulen line_number = 1;
	Ulen this_column = 1;
	Ulen last_column = 1;
	// We just count newlines from the beginning of the lexer stream up to but not
	// including where the token starts itself. We also keep track of the previous
	// lines length (last_column) to handle the case where a parse error happens
	// right on the end of a line (see below).
	for (Ulen i = 0; i < range.offset; i++) {
		if (m_lexer[i] == '\n') {
			line_number++;
			last_column = this_column;
			this_column = 0;
		}
		this_column++;
	}
	// When the error is right at the end of the line, the above counting logic
	// will report an error on the first column of the next line.
	if (this_column == 1 && line_number > 1) {
		line_number--;
		this_column = last_column;
	}
	fprintf(stderr, "%.*s:%zu:%zu: %s\n", (int)m_lexer.name().length(), m_lexer.name().data(), line_number, this_column, message);
}

AstExpr* Parser::parse_index_expr(AstExpr* operand) noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		ERROR("Expected '['");
		return nullptr;
	}
	auto beg_token = peek();
	next(); // Consume '['
	auto index = parse_expr();
	if (!index) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::RBRACKET) {
		ERROR("Expected ']'");
		return nullptr;
	}
	auto end_token = next(); // Consume ']'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstIndexExpr>(range, operand, index);
}

AstExpr* Parser::parse_binop_rhs(int expr_prec, AstExpr* lhs) noexcept {
	using Operator = AstBinExpr::Operator;
	for (;;) {
		auto peek_prec = peek().prec();
		if (peek_prec < expr_prec) {
			return lhs;
		}
		auto token = next();
		auto kind = token.kind; // Eat Op
		AstExpr* rhs = nullptr;
		if (kind == Token::Kind::KW_AS) {
			rhs = parse_type_expr();
		} else {
			rhs = parse_unary_expr();
		}
		if (!rhs) {
			return nullptr;
		}
		// Handle less-tight binding
		auto next_prec = peek().prec();
		if (peek_prec < next_prec) {
			rhs = parse_binop_rhs(peek_prec + 1, rhs);
			if (!rhs) {
				return nullptr;
			}
		}
		// Map the token 'kind' to an AstBin::Operator
		switch (kind) {
		/****/ case Token::Kind::PLUS:   lhs = new_node<AstBinExpr>(token.range, Operator::ADD,    lhs, rhs);
		break; case Token::Kind::MINUS:  lhs = new_node<AstBinExpr>(token.range, Operator::SUB,    lhs, rhs);
		break; case Token::Kind::STAR:   lhs = new_node<AstBinExpr>(token.range, Operator::MUL,    lhs, rhs);
		break; case Token::Kind::LT:     lhs = new_node<AstBinExpr>(token.range, Operator::LT,     lhs, rhs);
		break; case Token::Kind::LTE:    lhs = new_node<AstBinExpr>(token.range, Operator::LTE,    lhs, rhs);
		break; case Token::Kind::GT:     lhs = new_node<AstBinExpr>(token.range, Operator::GT,     lhs, rhs);
		break; case Token::Kind::GTE:    lhs = new_node<AstBinExpr>(token.range, Operator::GTE,    lhs, rhs);
		break; case Token::Kind::EQEQ:   lhs = new_node<AstBinExpr>(token.range, Operator::EQEQ,   lhs, rhs);
		break; case Token::Kind::NEQ:    lhs = new_node<AstBinExpr>(token.range, Operator::NEQ,    lhs, rhs);
		break; case Token::Kind::KW_AS:  lhs = new_node<AstBinExpr>(token.range, Operator::AS,     lhs, rhs);
		break; case Token::Kind::LOR:    lhs = new_node<AstBinExpr>(token.range, Operator::LOR,    lhs, rhs);
		break; case Token::Kind::LAND:   lhs = new_node<AstBinExpr>(token.range, Operator::LAND,   lhs, rhs);
		break; case Token::Kind::BOR:    lhs = new_node<AstBinExpr>(token.range, Operator::BOR,    lhs, rhs);
		break; case Token::Kind::BAND:   lhs = new_node<AstBinExpr>(token.range, Operator::BAND,   lhs, rhs);
		break; case Token::Kind::LSHIFT: lhs = new_node<AstBinExpr>(token.range, Operator::LSHIFT, lhs, rhs);
		break; case Token::Kind::RSHIFT: lhs = new_node<AstBinExpr>(token.range, Operator::RSHIFT, lhs, rhs);
		break;
		default:
			ERROR(token, "Unexpected token '%s' in binary expression", token.name());
			return nullptr;
		}
	}
}

// UnaryExpr
//	::= PrimaryExpr
//	::= '!' UnaryExpr
//	::= '-' UnaryExpr
//	::= '+' UnaryExpr
//	::= '*' UnaryExpr
//	::= '&' UnaryExpr
AstExpr* Parser::parse_unary_expr() noexcept {
	using Operator = AstUnaryExpr::Operator;
	AstExpr* operand = nullptr;
	auto token = peek();
	switch (token.kind) {
	case Token::Kind::NOT:
		next(); // Consume '!'
		if (!(operand = parse_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(token.range.include(operand->range), Operator::NOT, operand);
	case Token::Kind::MINUS:
		next(); // Consume '-'
		if (!(operand = parse_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(token.range, Operator::NEG, operand);
	case Token::Kind::PLUS:
		next(); // Consume '+'
		return parse_expr();
	case Token::Kind::STAR:
		next(); // Consume '*'
		if (!(operand = parse_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(token.range.include(operand->range), Operator::DEREF, operand);
	case Token::Kind::BAND:
		next(); // Consume '&'
		if (!(operand = parse_expr())) {
			return nullptr;
		}
		return new_node<AstUnaryExpr>(token.range.include(operand->range), Operator::ADDROF, operand);
	default:
		return parse_primary_expr();
	}
	BIRON_UNREACHABLE();
}

// Expr
//	::= PrimaryExpr BinOpRHS
//	::= PrimaryExpr IndexExpr
AstExpr* Parser::parse_expr() noexcept {
	auto lhs = parse_unary_expr();
	if (!lhs) {
		return nullptr;
	}
	if (peek().kind == Token::Kind::LBRACKET) {
		return parse_index_expr(lhs);
	}
	return parse_binop_rhs(0, lhs);
}

// PrimaryExpr
//	::= IdentExpr
//	::= IntExpr
//	::= StrExpr
//	::= TupleExpr
AstExpr* Parser::parse_primary_expr() noexcept {
	switch (peek().kind) {
	case Token::Kind::IDENT:
		return parse_ident_expr();
	case Token::Kind::LIT_INT:
		return parse_int_expr();
	case Token::Kind::LIT_STR:
		return parse_str_expr();
	case Token::Kind::LPAREN:
		return parse_tuple_expr();
	case Token::Kind::STAR:
		return parse_type_expr();
	default:
		ERROR("Unknown token '%s' in primary expression", peek().name());
		return nullptr;
	}
	BIRON_UNREACHABLE();
}

static Bool is_builtin_type(StringView ident) noexcept {
	if (ident == "Sint8")   return true;
	if (ident == "Uint8")   return true;
	if (ident == "Sint16")  return true;
	if (ident == "Uint16")  return true;
	if (ident == "Sint32")  return true;
	if (ident == "Uint32")  return true;
	if (ident == "Sint64")  return true;
	if (ident == "Uint64")  return true;
	if (ident == "Unit")    return true;
	if (ident == "Address") return true;
	if (ident == "String")  return true;
	return false;
}

// TypeExpr
//	::= Type
AstExpr* Parser::parse_type_expr() noexcept {
	auto token = peek();
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstTypeExpr>(token.range, type);
}

// IdentExpr
//	::= CallExpr
//  ::= TypeExpr
// CallExpr
//	::= Ident TupleExpr?
AstExpr* Parser::parse_ident_expr() noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	// TODO(dweiler): Support custom types
	if (is_builtin_type(m_lexer.string(peek().range))) {
		return parse_type_expr();
	}
	auto token = next();
	auto name = m_lexer.string(token.range);

	if (!has_symbol(name)) {
		ERROR("Undeclared symbol '%.*s'", (int)name.length(), name.data());
		return nullptr;
	}

	auto expr = new_node<AstVarExpr>(token.range, name);
	if (auto token = peek(); token.kind == Token::Kind::LPAREN) {
		auto args = parse_tuple_expr();
		if (!args) {
			return nullptr;
		}
		if (args->exprs.length() == 0) {
			args = nullptr;
		}
		return new_node<AstCallExpr>(token.range, expr, args, name == "printf");
	} else if (token.kind == Token::Kind::LBRACKET) {
		return parse_index_expr(expr);
	}
	return expr;
}

// IntExpr
//	::= '0x' HexDigit+
//	::= '0b' BinDigit+
//	::= DecDigit+
AstIntExpr* Parser::parse_int_expr() noexcept {
	if (peek().kind != Token::Kind::LIT_INT) {
		ERROR("Expected int");
		return nullptr;
	}
	auto token = next();
	auto lit = m_lexer.string(token.range);
	char* buf = lit.terminated(m_allocator);
	char* end = nullptr;
	Uint32 n = 0;
	if (lit.starts_with("0x")) {
		n = strtoul(buf, &end, 16);
	} else if (lit.starts_with("0b")) {
		n = strtoul(buf, &end, 2);
	} else {
		n = strtoul(buf, &end, 10);
	}
	return new_node<AstIntExpr>(token.range, n);
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
	return new_node<AstStrExpr>(token.range, literal);
}

AstAsmExpr* Parser::parse_asm_expr() noexcept {
	switch (peek().kind) {
	case Token::Kind::PERCENT:
		return parse_asm_reg_expr();
	case Token::Kind::DOLLAR:
		next(); // Consume '$'
		switch (peek().kind) {
		case Token::Kind::MINUS:
		case Token::Kind::PLUS:
		case Token::Kind::LIT_INT:
			return parse_asm_imm_expr();
		case Token::Kind::LPAREN:
			return parse_asm_sub_expr();
		default:
			ERROR("Unexpected token '%s' in asm expression", peek().name());
			return nullptr;
			break;
		}
	default:
		return parse_asm_mem_expr();
	}
	BIRON_UNREACHABLE();
}

AstAsmRegExpr* Parser::parse_asm_reg_expr() noexcept {
	if (peek().kind != Token::Kind::PERCENT) {
		ERROR("Expected '%%' in asm register");
		return nullptr;
	}
	auto beg = next(); // Consume '%'
	auto token = next(); // Consume IDENT
	auto name = m_lexer.string(token.range);
	AstAsmExpr* segment = nullptr;
	if (peek().kind == Token::Kind::COLON) {
		next(); // Consume ':'
		if (!(segment = parse_asm_expr())) {
			return nullptr;
		}
	}
	auto range = beg.range.include(segment->range);
	return new_node<AstAsmRegExpr>(range, name, segment);
}

AstAsmImmExpr* Parser::parse_asm_imm_expr() noexcept {
	// We already consumed '$' at this point.
	Bool neg = false;
	auto beg = peek();
	switch (peek().kind) {
	case Token::Kind::MINUS:
		next(); // Consume '-'
		neg = true;
		break;
	case Token::Kind::PLUS:
		next(); // Consume '+'
		break;
	default:
		break;
	}
	if (peek().kind != Token::Kind::LIT_INT) {
		ERROR("Expected integer literal in asm immediate");
		return nullptr;
	}
	auto expr = parse_int_expr();
	if (!expr) {
		return nullptr;
	}
	auto range = beg.range.include(expr->range);
	return new_node<AstAsmImmExpr>(range, neg, expr);
}

AstAsmMemExpr* Parser::parse_asm_mem_expr() noexcept {
	AstAsmExpr* base = nullptr;
	auto beg = peek();
	if (peek().kind != Token::Kind::LPAREN) {
		base = parse_asm_expr();
		if (!base) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return nullptr;
	}
	next(); // Consume '('
	AstAsmExpr* index = nullptr;
	AstAsmExpr* offset = nullptr;
	AstAsmExpr* size = nullptr;
	if (peek().kind == Token::Kind::PERCENT) {
		if (!(index = parse_asm_expr())) {
			return nullptr;
		}
	} else {
		// We expect imm ',' reg ',' imm
		if (!(offset = parse_asm_expr())) {
			return nullptr;
		}
		if (peek().kind != Token::Kind::COMMA) {
			ERROR("Expected ','");
			return nullptr;
		}
		next(); // Consume ','
		if (!(index = parse_asm_expr())) {
			return nullptr;
		}
		if (peek().kind != Token::Kind::COMMA) {
			ERROR("Expected ','");
			return nullptr;
		}
		next(); // Consume ','
		if (!(size = parse_asm_expr())) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::RPAREN) {
		ERROR("Expected ')'");
		return nullptr;
	}
	auto end = next(); // Consume ')'
	auto range = beg.range.include(end.range);
	return new_node<AstAsmMemExpr>(range, base, offset, index, size);
}

AstAsmSubExpr* Parser::parse_asm_sub_expr() noexcept {
	// We already consumed '$' at this point.
	auto beg = peek();
	if (peek().kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return nullptr;
	}
	next(); // Consume '('
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	auto token = next();
	auto selector = m_lexer.string(token.range);
	if (peek().kind != Token::Kind::DOT) {
		ERROR("Expected '.'");
		return nullptr;
	}
	next(); // Consume '.'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	token = next();
	auto field = m_lexer.string(token.range);
	// Modifier
	Maybe<StringView> modifier;
	if (peek().kind == Token::Kind::COLON) {
		next(); // Consume ':'
		if (peek().kind != Token::Kind::IDENT) {
			ERROR("Expected identifier");
			return nullptr;
		}
		token = next();
		modifier = m_lexer.string(token.range);
	}
	if (peek().kind != Token::Kind::RPAREN) {
		ERROR("Expected ')'");
		return nullptr;
	}
	auto end = next(); // Consume ')'
	auto range = beg.range.include(end.range);
	return new_node<AstAsmSubExpr>(range, selector, field, move(modifier));
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
		auto expr = parse_expr();
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
		ERROR("Expected ')'");
		return nullptr;
	}
	next(); // Consume ')'
	auto range = beg_token.range.include(end_token.range);
	return new_node<AstTupleExpr>(range, move(exprs));
}

// Type
//	::= IdentType
//	::= TupleType
//	::= VarArgsType
//	::= PtrType
//	::= BracketType
AstType* Parser::parse_type() noexcept {
	switch (peek().kind) {
	case Token::Kind::IDENT:
		return parse_ident_type();
	case Token::Kind::LPAREN:
		return parse_tuple_type();
	case Token::Kind::ELLIPSIS:
		return parse_varargs_type();
	case Token::Kind::STAR:
		return parse_ptr_type();
	case Token::Kind::LBRACKET:
		return parse_bracket_type();
	default:
		ERROR("Unexpected token '%s' in type", peek().name());
		return nullptr;
	}
	BIRON_UNREACHABLE();
}

// IdentType
//	::= Ident
AstIdentType* Parser::parse_ident_type() noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected identifier");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range);
	return new_node<AstIdentType>(name);
}

// TupleType
//	::= '(' Elem ( ',' Elem )* ')'
// Elem
//	::= (Ident ':')? Type
AstTupleType* Parser::parse_tuple_type() noexcept {
	if (peek().kind != Token::Kind::LPAREN) {
		ERROR("Expected '('");
		return nullptr;
	}
	next(); // Consume '('
	Array<AstTupleType::Elem> elems{m_allocator};
	while (peek().kind != Token::Kind::RPAREN) {
		if (peek().kind != Token::Kind::IDENT) {
			ERROR("Expected identifier in tuple type");
			return nullptr;
		} 
		auto token = next();
		auto name = m_lexer.string(token.range); // Consume Ident
		if (peek().kind == Token::Kind::COLON) {
			next(); // Consume ':'
			auto type = parse_type();
			if (!type) {
				return nullptr;
			}
			if (!elems.emplace_back(move(name), type)) {
				ERROR("Out of memory");
				return nullptr;
			}
		} else {
			// Would be a regular ident type
			auto type = new_node<AstIdentType>(name);
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
		ERROR("Expected ')'");
		return nullptr;
	}
	next(); // Consume ')'
	return new_node<AstTupleType>(move(elems));
}

// VarArgsType
//	::= '...'
AstVarArgsType* Parser::parse_varargs_type() noexcept {
	if (peek().kind != Token::Kind::ELLIPSIS) {
		ERROR("Expected '...'");
		return nullptr;
	}
	next(); // Consume '...'
	return new_node<AstVarArgsType>();
}

// PtrType
//	::= '*' Type
AstPtrType* Parser::parse_ptr_type() noexcept {
	if (peek().kind != Token::Kind::STAR) {
		ERROR("Expected '*'");
		return nullptr;
	}
	next(); // Consume '*'
	auto type = parse_type();
	if (!type) {
		return nullptr;
	}
	return new_node<AstPtrType>(type);
}

// BracketType
//	::= ArrayType
//	::= SliceType
// ArrayType
//	::= '[' Expr ']' Type
// SliceType
//	::= '[' ']'
AstType* Parser::parse_bracket_type() noexcept {
	if (peek().kind != Token::Kind::LBRACKET) {
		ERROR("Expected '['");
		return nullptr;
	}
	next(); // Consume '['
	AstExpr* expr = nullptr;
	if (peek().kind != Token::Kind::RBRACKET) {
		if (!(expr = parse_expr())) {
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
	if (expr) {
		return new_node<AstArrayType>(type, expr);
	} else {
		return new_node<AstSliceType>(type);
	}
	BIRON_UNREACHABLE();
}

// Stmt
//	::= BlockStmt
//	::= ReturnStmt
//	::= DeferStmt
//	::= IfStmt
//	::= LetStmt
//	::= ExprStmt
//	::= ForStmt
AstStmt* Parser::parse_stmt() noexcept {
	switch (peek().kind) {
	case Token::Kind::LBRACE:
		return parse_block_stmt();
	case Token::Kind::KW_RETURN:
		return parse_return_stmt();
	case Token::Kind::KW_DEFER:
		return parse_defer_stmt();
	case Token::Kind::KW_IF:
		return parse_if_stmt();
	case Token::Kind::KW_LET:
		return parse_let_stmt();
	case Token::Kind::KW_FOR:
		return parse_for_stmt();
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
	next(); // Consume '{'
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
	next(); // Consume '}'
	auto node = new_node<AstBlockStmt>(move(stmts));
	if (!node) {
		return nullptr;
	}
	m_scope = m_scope->prev;
	return node;
}

// ReturnStmt
//	::= 'return' Expr? ';'
AstReturnStmt* Parser::parse_return_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_RETURN) {
		ERROR("Expected 'return'");
		return nullptr;
	}
	next(); // Consume 'return'
	AstExpr* expr = nullptr;
	if (peek().kind != Token::Kind::SEMI) {
		expr = parse_expr();
		if (!expr) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after return statement");
		return nullptr;
	}
	next(); // Consume ';'
	return new_node<AstReturnStmt>(expr);
}

// DeferStmt
//	::= 'defer' Stmt
AstDeferStmt* Parser::parse_defer_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_DEFER) {
		ERROR("Expected 'defer'");
		return nullptr;
	}
	next(); // Consume 'defer'
	auto stmt = parse_stmt();
	if (!stmt) {
		return nullptr;
	}
	return new_node<AstDeferStmt>(stmt);
};

// IfStmt
//	::= 'if' (LetStmt ';')? Expr BlockStmt ('else' (IfStmt | BlockStmt))?
AstIfStmt* Parser::parse_if_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_IF) {
		ERROR("Expected 'if'");
		return nullptr;
	}
	next(); // Consume 'if'
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
	if (peek().kind == Token::Kind::KW_LET) {
		m_scope = &scope;
		if (!(init = parse_let_stmt())) {
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
	auto node = new_node<AstIfStmt>(init, expr, then, elif);
	if (!node) {
		return nullptr;
	}
	m_scope = m_scope->prev;
	return node;
}

// LetStmt
//	::= 'let' Ident ('=' Expr)? ';'
AstLetStmt* Parser::parse_let_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_LET) {
		ERROR("Expected 'let'");
		return nullptr;
	}
	next(); // Consume 'let'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected name for let");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range);
	if (has_symbol(name)) {
		ERROR("Duplicate symbol '%.*s'", (int)name.length(), name.data());
		return nullptr;
	}
	AstExpr* init = nullptr;
	if (peek().kind != Token::Kind::EQ) {
		ERROR("Expected expression");
		return nullptr;
	}
	next(); // Consume '='
	init = parse_expr();
	if (!init) {
		return nullptr;
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';' after let statement");
		return nullptr;
	}
	next(); // Consume ';'
	auto node = new_node<AstLetStmt>(name, init);
	if (!node) {
		return nullptr;
	}
	if (!m_scope->lets.push_back(node)) {
		ERROR("Out of memory");
		return nullptr;
	}
	return node;
}

// ForStmt
//	::= 'for' BlockStmt
//	::= 'for' LetStmt? Expr BlockStmt
AstForStmt* Parser::parse_for_stmt() noexcept {
	if (peek().kind != Token::Kind::KW_FOR) {
		ERROR("Expected 'for'");
		return nullptr;
	}
	next(); // Consume 'for'
	AstLetStmt* let = nullptr;
	AstExpr* expr = nullptr;
	Scope scope{m_allocator, m_scope};
	if (peek().kind == Token::Kind::KW_LET) {
		m_scope = &scope;
		if (!(let = parse_let_stmt())) {
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
	auto node = new_node<AstForStmt>(let, expr, post, body);
	if (!node) {
		return nullptr;
	}
	m_scope = m_scope->prev;
	return node;
}

// ExprStmt
//	::= Expr ';'
//	::= Expr '=' Expr ';'
AstStmt* Parser::parse_expr_stmt(Bool semi) noexcept {
	auto expr = parse_expr();
	if (!expr) {
		return nullptr;
	}
	// TODO(dweiler): compound operators
	AstAssignStmt* assignment = nullptr;
	if (peek().kind == Token::Kind::EQ) {
		next(); // Consume '='
		auto value = parse_expr();
		if (!value) {
			return nullptr;
		}
		assignment = new_node<AstAssignStmt>(expr, value, AstAssignStmt::StoreOp::WR);
	}
	if (semi) {
		if (peek().kind != Token::Kind::SEMI) {
			ERROR("Expected ';' after statement");
			return nullptr;
		}
		next(); // Consume ';'
	}
	if (assignment) {
		return assignment;
	}
	return new_node<AstExprStmt>(expr);
}

// AsmStmt
//	::= Ident (AsmExpr (',' AsmExpr)*)? ';'
AstAsmStmt* Parser::parse_asm_stmt() noexcept {
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected mnemonic");
		return nullptr;
	}
	auto token = next();
	auto mnemonic = m_lexer.string(token.range);
	Array<AstAsmExpr*> exprs{m_allocator};
	while (peek().kind != Token::Kind::SEMI) {
		auto expr = parse_asm_expr();
		if (!expr) {
			return nullptr;
		}
		if (!exprs.push_back(expr)) {
			ERROR("Out of memory");
			return nullptr;
		}
		if (peek().kind == Token::Kind::COMMA) {
			next(); // Consume ','
			if (peek().kind == Token::Kind::SEMI) {
				ERROR("Expected expression");
				return nullptr;
			}
		} else {
			break;
		}
	}
	if (peek().kind != Token::Kind::SEMI) {
		ERROR("Expected ';'");
		return nullptr;
	}
	next(); // Consume ';'
	return new_node<AstAsmStmt>(mnemonic, move(exprs));
}

// Fn
//	::= 'fn' TupleType? Ident TupleType ('->' Type)? BlockStmt
AstFn* Parser::parse_fn() noexcept {
	Scope scope{m_allocator, m_scope};
	m_scope = &scope;
	if (peek().kind != Token::Kind::KW_FN) {
		ERROR("Expected 'fn'");
		return nullptr;
	}
	next(); // Consume 'fn'
	AstTupleType* generic = nullptr;
	if (peek().kind == Token::Kind::LPAREN) {
		generic = parse_tuple_type();
	}
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected name for 'fn'");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range); // Consume Ident
	auto type = parse_tuple_type();
	if (!type) {
		return nullptr;
	}
	if (type->elems.length() == 0) {
		type = nullptr;
	} else {
		m_scope->args = type;
	}
	AstType* rtype = nullptr;
	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		rtype = parse_type();
	}
	AstBlockStmt* body = parse_block_stmt();
	if (!body) {
		return nullptr;
	}
	auto node = new_node<AstFn>(generic, name, type, rtype, body);
	if (!node) {
		return nullptr;
	}
	m_scope = m_scope->prev;
	return node;
}

// Asm
//	::= 'asm' Ident TupleType ('->' Type)? '{' AsmStmt* '}'
AstAsm* Parser::parse_asm() noexcept {
	if (peek().kind != Token::Kind::KW_ASM) {
		ERROR("Expected 'asm'");
		return nullptr;
	}
	next(); // Consume 'asm'
	if (peek().kind != Token::Kind::IDENT) {
		ERROR("Expected name for 'asm'");
		return nullptr;
	}
	auto token = next();
	auto name = m_lexer.string(token.range);
	auto type = parse_tuple_type();
	if (!type) {
		return nullptr;
	}
	AstType* clobbers = nullptr;
	if (peek().kind == Token::Kind::ARROW) {
		next(); // Consume '->'
		clobbers = parse_type();
		if (!clobbers) {
			return nullptr;
		}
	}
	if (peek().kind != Token::Kind::LBRACE) {
		ERROR("Expected '{");
		return nullptr;
	}
	next(); // Consume '}'
	Array<AstAsmStmt*> stmts{m_allocator};
	while (peek().kind != Token::Kind::RBRACE) {
		auto stmt = parse_asm_stmt();
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
	next(); // Consume '}'
	return new_node<AstAsm>(name, type, clobbers, move(stmts));
}

Maybe<Unit> Parser::parse() noexcept {
	Unit unit{m_allocator};
	for (;;) switch (peek().kind) {
	case Token::Kind::KW_FN:
		if (auto fn = parse_fn()) {
			if (!unit.fns.push_back(fn)) {
				ERROR("Out of memory");
				return None{};
			}
		}
		break;
	case Token::Kind::KW_ASM:
		if (auto a = parse_asm()) {
			if (!unit.asms.push_back(a)) {
				ERROR("Out of memory");
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