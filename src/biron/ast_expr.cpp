#include <biron/ast_expr.h>
#include <biron/ast_type.h>

namespace Biron {

void AstTupleExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	Bool f = true;
	for (auto expr : m_exprs) {
		if (!f) builder.append(", ");
		expr->dump(builder);
		f = false;
	}
	builder.append(')');
}

void AstCallExpr::dump(StringBuilder& builder) const noexcept {
	m_callee->dump(builder);
	if (m_args) {
		m_args->dump(builder);
	} else {
		builder.append("()");
	}
}

void AstTypeExpr::dump(StringBuilder& builder) const noexcept {
	m_type->dump(builder);
}

void AstVarExpr::dump(StringBuilder& builder) const noexcept {
	builder.append(m_name);
}

void AstIntExpr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::U8:
		builder.append(m_as_u8);
		builder.append("_u8");
		break;
	case Kind::U16:
		builder.append(m_as_u16);
		builder.append("_u16");
		break;
	case Kind::U32:
		builder.append(m_as_u32);
		builder.append("_u32");
		break;
	case Kind::U64:
		builder.append(m_as_u64);
		builder.append("_u64");
		break;
	case Kind::S8:
		builder.append(m_as_s8);
		builder.append("_s8");
		break;
	case Kind::S16:
		builder.append(m_as_s16);
		builder.append("_s16");
		break;
	case Kind::S32:
		builder.append(m_as_s32);
		builder.append("_s32");
		break;
	case Kind::S64:
		builder.append(m_as_s64);
		builder.append("_s64");
		break;
	}
}

void AstFltExpr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::F32:
		builder.append(m_as_f32);
		builder.append("_f32");
		break;
	case Kind::F64:
		builder.append(m_as_f64);
		builder.append("_f64");
		break;
	}
}

void AstStrExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('"');
	builder.append(m_literal);
	builder.append('"');
}

void AstBoolExpr::dump(StringBuilder& builder) const noexcept {
	if (m_value) {
		builder.append("true");
	} else {
		builder.append("false");
	}
}

void AstAggExpr::dump(StringBuilder& builder) const noexcept {
	m_type->dump(builder);
	builder.append('{');
	Bool f = true;
	for (const auto &expr : m_exprs) {
		if (!f) builder.append(", ");
		expr->dump(builder);
		f = false;
	}
	builder.append('}');
}

void AstBinExpr::dump(StringBuilder& builder) const noexcept {
	m_lhs->dump(builder);
	switch (m_op) {
	/****/ case Op::ADD:    builder.append(" + ");
	break; case Op::SUB:    builder.append(" - ");
	break; case Op::MUL:    builder.append(" * ");
	break; case Op::DIV:    builder.append(" / ");
	break; case Op::EQ:     builder.append(" == ");
	break; case Op::NE:     builder.append(" != ");
	break; case Op::GT:     builder.append(" > ");
	break; case Op::GE:     builder.append(" >= ");
	break; case Op::LT:     builder.append(" < ");
	break; case Op::LE:     builder.append(" <= ");
	break; case Op::AS:     builder.append(" as ");
	break; case Op::LOR:    builder.append(" || ");
	break; case Op::LAND:   builder.append(" && ");
	break; case Op::BOR:    builder.append(" | ");
	break; case Op::BAND:   builder.append(" & ");
	break; case Op::LSHIFT: builder.append(" << ");
	break; case Op::RSHIFT: builder.append(" >> ");
	break; case Op::DOT:    builder.append('.');
	break;
	}
	m_rhs->dump(builder);
}

void AstUnaryExpr::dump(StringBuilder& builder) const noexcept {
	switch (m_op) {
	/****/ case Op::NEG:    builder.append('-');
	break; case Op::NOT:    builder.append('!');
	break; case Op::DEREF:  builder.append('*');
	break; case Op::ADDROF: builder.append('&');
	}
	m_operand->dump(builder);
}

void AstIndexExpr::dump(StringBuilder& builder) const noexcept {
	m_operand->dump(builder);
	builder.append('[');
	m_index->dump(builder);
	builder.append(']');
}

void AstExplodeExpr::dump(StringBuilder& builder) const noexcept {
	builder.append("...");
	m_operand->dump(builder);
}

} // namespace Biron