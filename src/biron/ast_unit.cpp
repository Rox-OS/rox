#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>

namespace Biron {

void AstModule::dump(StringBuilder& builder) const noexcept {
	builder.append("module");
	builder.append(' ');
	builder.append(m_name);
	builder.append(';');
	builder.append('\n');
}

void AstFn::dump(StringBuilder& builder, int depth) const noexcept {
	builder.append("fn");
	m_objs->dump(builder);
	builder.append(' ');
	builder.append(m_name);
	m_args->dump(builder);
	if (m_effects.length()) {
		builder.append(' ');
		builder.append('<');
		Bool f = true;
		for (auto effect : m_effects) {
			effect->dump(builder);
			if (!f) builder.append(", ");
			f = false;
		}
		builder.append('>');
	}
	builder.append(' ');
	builder.append("->");
	builder.append(' ');
	m_ret->dump(builder);
	m_body->dump(builder, depth);
}

Ast::~Ast() noexcept {
	for (auto& cache : m_caches) if (cache) {
		for (auto node : *cache) {
			static_cast<AstNode*>(node)->~AstNode();
		}
	}
}

void Ast::dump(StringBuilder& builder) const noexcept {
	static_cast<const AstModule*>((*m_caches[AstID::id<AstModule>()])[0])->dump(builder);
	const auto& fns = m_caches[AstID::id<AstFn>()];
	if (fns) for (auto fn : *fns) {
		static_cast<const AstFn*>(fn)->dump(builder, 0);
	}
}

} // namespace Biron