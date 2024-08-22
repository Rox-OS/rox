#ifndef BIRON_AST_ATTR_H
#define BIRON_AST_ATTR_H
#include <biron/ast.h>
#include <biron/util/string.inl>

namespace Biron {

struct AstAttr : AstNode {
	static inline constexpr auto KIND = Kind::ATTR;
	enum class Kind { SECTION, ALIGN };
	constexpr AstAttr(Kind kind, Range range) noexcept
		: AstNode{KIND, range}
		, m_kind{kind}
	{
	}
	virtual ~AstAttr() noexcept = default;
	template<DerivedFrom<AstAttr> T>
	[[nodiscard]] constexpr Bool is_attr() const noexcept {
		return m_kind == T::KIND;
	}
	virtual void dump(StringBuilder& builder) const noexcept = 0;
private:
	Kind m_kind;
};

struct AstSectionAttr : AstAttr {
	static inline constexpr auto KIND = Kind::SECTION;
	constexpr AstSectionAttr(StringView name, Range range) noexcept
		: AstAttr{KIND, range}
		, m_name{name}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
private:
	StringView m_name;
};

struct AstAlignAttr : AstAttr {
	static inline constexpr auto KIND = Kind::ALIGN;
	constexpr AstAlignAttr(Uint64 align, Range range) noexcept
		: AstAttr{KIND, range}
		, m_align{align}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
private:
	Uint64 m_align;
};

} // namespace Biron

#endif // BIRON_AST_ATTR_H