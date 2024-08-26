#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>

namespace Biron {

void AstTopFn::dump(StringBuilder& builder, int depth) const noexcept {
	builder.append("fn");
	builder.append(' ');
	builder.append(m_name);
	if (m_args) {
		m_args->dump(builder);
	} else {
		builder.append("()");
	}
	builder.append(" -> ");
	if (m_rets) {
		m_rets->dump(builder);
	} else {
		builder.append("Unit");
	}
	m_body->dump(builder, depth);
}

} // namespace Biron