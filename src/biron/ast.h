#ifndef BIRON_AST_H
#define BIRON_AST_H
#include <biron/util/numeric.inl>
#include <biron/util/traits/is_base_of.inl>

namespace Biron {

struct AstNode {
	enum class Kind : Uint8 {
		TYPE, EXPR, STMT, FN, ASM, ATTR
	};
	constexpr AstNode(Kind kind, Range range) noexcept
		: m_kind{kind}
		, m_range{range}
	{
	}
	virtual ~AstNode() noexcept = default;
	template<DerivedFrom<AstNode> T>
	[[nodiscard]] constexpr Bool is_node() const noexcept {
		return m_kind = T::KIND;
	}
	[[nodiscard]] constexpr const Range& range() const noexcept {
		return m_range;
	}
private:
	Kind m_kind;
	Range m_range;
};

} // namespace Biron

#endif // BIRON_AST_H