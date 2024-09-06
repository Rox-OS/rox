#ifndef BIRON_AST_ATTR_H
#define BIRON_AST_ATTR_H
#include <biron/ast.h>
#include <biron/ast_const.h>

#include <biron/util/string.h>

namespace Biron {

struct AstExpr;

struct AstAttr : AstNode {
	static inline constexpr const auto KIND = Kind::ATTR;
	constexpr AstAttr(StringView name, AstExpr* expr, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_expr{expr}
	{
	}
	virtual ~AstAttr() noexcept = default;
	template<DerivedFrom<AstAttr> T>
	[[nodiscard]] constexpr Bool is_attr() const noexcept {
		return m_kind == T::KIND;
	}
	void dump(StringBuilder& builder) const noexcept;
	Maybe<AstConst> eval(Cg& cg) const noexcept;
	StringView name() const noexcept { return m_name; }
private:
	StringView m_name;
	AstExpr*   m_expr;
};

} // namespace Biron

#endif // BIRON_AST_ATTR_H