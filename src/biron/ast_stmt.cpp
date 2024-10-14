#include <biron/ast_stmt.h>
#include <biron/ast_expr.h>

#include <biron/cg.h>

#include <biron/util/unreachable.inl>

namespace Biron {

const char* AstStmt::name() const noexcept {
	switch (m_kind) {
	case Kind::BLOCK:    return "BLOCK";
	case Kind::RETURN:   return "RETURN";
	case Kind::DEFER:    return "DEFER";
	case Kind::BREAK:    return "BREAK";
	case Kind::CONTINUE: return "CONTINUE";
	case Kind::IF:       return "IF";
	case Kind::LLET:     return "LLET";
	case Kind::GLET:     return "GLET";
	case Kind::USING:    return "USING";
	case Kind::FOR:      return "FOR";
	case Kind::EXPR:     return "EXPR";
	case Kind::ASSIGN:   return "ASSIGN";
	}
	BIRON_UNREACHABLE();
}

Bool AstStmt::codegen(Cg& cg) const noexcept {
	return cg.error(range(), "Unsupported codegen for %s", name());
}

void AstBlockStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.append('\n');
	builder.repeat('\t', depth);
	builder.append("{\n");
	for (auto stmt : m_stmts) {
		stmt->dump(builder, depth + 1);
	}
	builder.repeat('\t', depth);
	builder.append("}\n");
}

void AstReturnStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("return");
	builder.append(' ');
	if (m_expr) {
		m_expr->dump(builder);
	}
	builder.append(';');
	builder.append('\n');
}

void AstDeferStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("defer");
	builder.append('\n');
	builder.repeat('\t', depth);
	builder.append("{\n");
	m_stmt->dump(builder, depth + 1);
	builder.repeat('\t', depth);
	builder.append("}\n");
}

void AstBreakStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("break");
	builder.append(';');
}

void AstContinueStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("continue");
	builder.append(';');
}

void AstIfStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("if");
	builder.append(' ');
	if (m_init) {
		m_init->dump(builder, 0);
		builder.pop();
		builder.append(' ');
	}
	m_expr->dump(builder);
	builder.repeat('\t', depth);
	m_then->dump(builder, depth);
	if (m_elif) {
		builder.repeat('\t', depth);
		builder.append("else");
		builder.append(' ');
		m_elif->dump(builder, depth);
	}
}

void AstLLetStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("let");
	builder.append(' ');
	builder.append(m_name);
	builder.append(" = ");
	m_init->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstGLetStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("let");
	builder.append(' ');
	builder.append(m_name);
	builder.append(" = ");
	m_init->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstUsingStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("using");
	builder.append(' ');
	builder.append(m_name);
	builder.append(" = ");
	m_init->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstForStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("for");
	builder.append(' ');
	if (m_init) {
		m_init->dump(builder, 0);
		builder.pop();
		builder.append(' ');
	}
	if (m_expr) {
		m_expr->dump(builder);
	}
	if (m_post) {
		builder.append(';');
		builder.append(' ');
		m_post->dump(builder, 0);
		builder.pop(); // Remove '\n'
		builder.pop(); // Remove ';'
	}
	m_body->dump(builder, depth);
}

void AstExprStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	m_expr->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstAssignStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	m_dst->dump(builder);
	builder.append(" = ");
	m_src->dump(builder);
	builder.append(';');
	builder.append('\n');
}

} // namespace Biron