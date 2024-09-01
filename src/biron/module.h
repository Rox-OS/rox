#ifndef BIRON_MODULE_H
#define BIRON_MODULE_H
#include <biron/ast_unit.h>

namespace Biron {

struct Module {
	constexpr Module(Allocator& allocator, StringView name)
		: m_units{allocator}
		, m_name{name}
	{
	}
private:
	Array<AstUnit> m_units;
	StringView     m_name;
};

} // namespace Biron

#endif // BIRON_MODULE_H