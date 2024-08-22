#include <biron/ast_attr.h>

namespace Biron {

void AstSectionAttr::dump(StringBuilder& builder) const noexcept {
	builder.append('@');
	builder.append('(');
	builder.append("section");
	builder.append('(');
	builder.append('"');
	builder.append(m_name);
	builder.append('"');
	builder.append(')');
	builder.append(')');
}

void AstAlignAttr::dump(StringBuilder& builder) const noexcept {
	builder.append('@');
	builder.append('(');
	builder.append("align");
	builder.append('(');
	builder.append('"');
	builder.append(m_align);
	builder.append('"');
	builder.append(')');
	builder.append(')');
}

} // namespace Biron