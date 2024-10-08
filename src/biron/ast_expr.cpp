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
	m_args->dump(builder);
}

void AstTypeExpr::dump(StringBuilder& builder) const noexcept {
	m_type->dump(builder);
}

void AstVarExpr::dump(StringBuilder& builder) const noexcept {
	builder.append(m_name);
}

void AstSelectorExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('.');
	builder.append(m_name);
}

void AstIntExpr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::U8:
		builder.append(Uint8(m_as_uint));
		builder.append("_u8");
		break;
	case Kind::U16:
		builder.append(Uint16(m_as_uint));
		builder.append("_u16");
		break;
	case Kind::U32:
		builder.append(Uint32(m_as_uint));
		builder.append("_u32");
		break;
	case Kind::U64:
		builder.append(Uint64(m_as_uint));
		builder.append("_u64");
		break;
	case Kind::S8:
		builder.append(Sint8(m_as_sint));
		builder.append("_s8");
		break;
	case Kind::S16:
		builder.append(Sint16(m_as_sint));
		builder.append("_s16");
		break;
	case Kind::S32:
		builder.append(Sint32(m_as_sint));
		builder.append("_s32");
		break;
	case Kind::S64:
		builder.append(Sint32(m_as_sint));
		builder.append("_s64");
		break;
	case Kind::UNTYPED:
		builder.append(Uint64(m_as_uint));
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
	case Kind::UNTYPED:
		builder.append(m_as_f64);
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
	builder.append("new");
	builder.append(' ');
	m_type->dump(builder);
	builder.append(' ');
	builder.append('{');
	builder.append(' ');
	Bool f = true;
	for (const auto &expr : m_exprs) {
		if (!f) builder.append(", ");
		expr->dump(builder);
		f = false;
	}
	builder.append(' ');
	builder.append('}');
}

void AstBinExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	m_lhs->dump(builder);
	builder.append(' ');
	switch (m_op) {
	/****/ case Op::ADD:    builder.append('+');
	break; case Op::SUB:    builder.append('-');
	break; case Op::MUL:    builder.append('*');
	break; case Op::DIV:    builder.append('/');
	break; case Op::EQ:     builder.append("==");
	break; case Op::NE:     builder.append("!=");
	break; case Op::GT:     builder.append(">");
	break; case Op::GE:     builder.append(">=");
	break; case Op::MAX:    builder.append(">?");
	break; case Op::LT:     builder.append('<');
	break; case Op::LE:     builder.append("<=");
	break; case Op::MIN:    builder.append("<?");
	break; case Op::BOR:    builder.append('|');
	break; case Op::BAND:   builder.append('&');
	break; case Op::LSHIFT: builder.append("<<");
	break; case Op::RSHIFT: builder.append(">>");
	break;
	}
	builder.append(' ');
	m_rhs->dump(builder);
	builder.append(')');
}

void AstLBinExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	m_lhs->dump(builder);
	builder.append(' ');
	switch (m_op) {
	/****/ case Op::LOR:  builder.append("||");
	break; case Op::LAND: builder.append("&&");
	}
	builder.append(' ');
	m_rhs->dump(builder);
	builder.append(')');
}

void AstUnaryExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	switch (m_op) {
	/****/ case Op::NEG:    builder.append('-');
	break; case Op::NOT:    builder.append('!');
	break; case Op::DEREF:  builder.append('*');
	break; case Op::ADDROF: builder.append('&');
	}
	m_operand->dump(builder);
	builder.append(')');
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

void AstEffExpr::dump(StringBuilder& builder) const noexcept {
	m_operand->dump(builder);
	builder.append('!');
}

void AstInferSizeExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('?');
}

void AstAccessExpr::dump(StringBuilder& builder) const noexcept {
	m_lhs->dump(builder);
	builder.append('.');
	m_rhs->dump(builder);
}

void AstCastExpr::dump(StringBuilder& builder) const noexcept {
	m_operand->dump(builder);
	builder.append(' ');
	builder.append("as");
	builder.append(' ');
	m_type->dump(builder);
}

void AstTestExpr::dump(StringBuilder& builder) const noexcept {
	m_operand->dump(builder);
	builder.append(' ');
	builder.append("is");
	builder.append(' ');
	m_type->dump(builder);
}

void AstPropExpr::dump(StringBuilder& builder) const noexcept {
	m_prop->dump(builder);
	builder.append(' ');
	builder.append("of");
	builder.append(' ');
	m_expr->dump(builder);
}

} // namespace Biron