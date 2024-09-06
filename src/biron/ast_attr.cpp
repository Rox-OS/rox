#include <biron/ast_attr.h>
#include <biron/ast_expr.h>

namespace Biron {

void AstAttr::dump(StringBuilder& builder) const noexcept {
	builder.append(m_name);
	builder.append('(');
	m_expr->dump(builder);
	builder.append(')');
}

Maybe<AstConst> AstAttr::eval(Cg& cg) const noexcept {
	return m_expr->eval_value(cg);
}

} // namespace Biron
