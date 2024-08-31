#include <biron/ast_attr.h>

namespace Biron {

void AstSectionAttr::dump(StringBuilder& builder) const noexcept {
	builder.append("section");
	builder.append('(');
	builder.append('"');
	builder.append(m_name);
	builder.append('"');
	builder.append(')');
}

void AstAlignAttr::dump(StringBuilder& builder) const noexcept {
	builder.append("align");
	builder.append('(');
	builder.append('"');
	builder.append(m_align);
	builder.append('"');
	builder.append(')');
}

void AstUsedAttr::dump(StringBuilder& builder) const noexcept {
	builder.append("used");
	builder.append('(');
	if (m_value) {
		builder.append("true");
	} else {
		builder.append("false");
	}
	builder.append(')');
}

void AstInlineAttr::dump(StringBuilder& builder) const noexcept {
	builder.append("inline");
	builder.append('(');
	if (m_value) {
		builder.append("true");
	} else {
		builder.append("false");
	}
	builder.append(')');
}

void AstAliasableAttr::dump(StringBuilder& builder) const noexcept {
	builder.append("aliasable");
	builder.append('(');
	if (m_value) {
		builder.append("true");
	} else {
		builder.append("false");
	}
	builder.append(')');
}

} // namespace Biron