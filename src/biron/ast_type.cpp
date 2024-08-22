#include <biron/ast_type.h>
#include <biron/ast_expr.h>

namespace Biron {

void AstTupleType::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	for (Ulen l = m_elems.length(), i = 0; i < l; i++) {
		const auto &elem = m_elems[i];
		if (elem.name()) {
			builder.append(*elem.name());
		} else {
			builder.append(i);
		}
		builder.append(':');
		builder.append(' ');
		elem.type()->dump(builder);
		if (i + 1 != l) {
			builder.append(", ");
		}
	}
	builder.append(')');
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

} // namespace Biron