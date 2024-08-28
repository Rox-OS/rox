#include <biron/ast_type.h>
#include <biron/ast_expr.h>

namespace Biron {

void AstTupleType::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	Bool f = true;
	for (const auto& elem : m_elems) {
		if (!f) builder.append(", ");
		if (elem.name()) {
			builder.append(*elem.name());
			builder.append(':');
			builder.append(' ');
		}
		elem.type()->dump(builder);
		f = false;
	}
	builder.append(')');
}

void AstUnionType::dump(StringBuilder& builder) const noexcept {
	Bool f = true;
	for (const auto type : m_types) {
		if (!f) builder.append(" | ");
		type->dump(builder);
		f = false;
	}
}

void AstIdentType::dump(StringBuilder& builder) const noexcept {
	builder.append(m_ident);
}

void AstVarArgsType::dump(StringBuilder& builder) const noexcept {
	builder.repeat('.', 3);
}

void AstPtrType::dump(StringBuilder& builder) const noexcept {
	builder.append('*');
	m_type->dump(builder);
}

void AstArrayType::dump(StringBuilder& builder) const noexcept {
	builder.append('[');
	m_extent->dump(builder);
	builder.append(']');
	m_type->dump(builder);
}

void AstSliceType::dump(StringBuilder& builder) const noexcept {
	builder.append("[]");
	m_type->dump(builder);
}

void AstFnType::dump(StringBuilder& builder) const noexcept {
	builder.append("fn");
	m_args->dump(builder);
	builder.append(" -> ");
	m_rets->dump(builder);
}

} // namespace Biron