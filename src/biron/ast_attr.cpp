#include <biron/ast_attr.h>

namespace Biron {

void AstBoolAttr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::USED:
		builder.append("used");
		break;
	case Kind::INLINE:
		builder.append("inline");
		break;
	case Kind::ALIASABLE:
		builder.append("aliasable");
		break;
	case Kind::REDZONE:
		builder.append("redzone");
		break;
	case Kind::EXPORT:
		builder.append("export");
		break;
	}
	builder.append('(');
	if (m_value) {
		builder.append("true");
	} else {
		builder.append("false");
	}
	builder.append(')');
}

void AstIntAttr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::ALIGN:
		builder.append("align");
		break;
	case Kind::ALIGNSTACK:
		builder.append("alignstack");
		break;
	}
	builder.append('(');
	builder.append(m_value);
	builder.append(')');
}

void AstStringAttr::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::SECTION:
		builder.append("section");
		break;
	}
	builder.append('(');
	builder.append(m_value);
	builder.append(')');
}

} // namespace Biron
