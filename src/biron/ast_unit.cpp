#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>

namespace Biron {

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
	m_rets->dump(builder);
	m_body->dump(builder, depth);
}

Bool AstUnit::add_import(AstImport* import) noexcept {
	for (auto existing : m_imports) {
		if (existing->name() == import->name()) {
			// Duplicate import
			return false;
		}
	}
	return m_imports.push_back(import);
}

Bool AstUnit::assign_module(AstModule* module) noexcept {
	if (m_module) {
		// Cannot have more than once instance of 'module' in a unit.
		return false;
	}
	m_module = module;
	return true;
}

} // namespace Biron