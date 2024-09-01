#ifndef BIRON_AST_ATTR_H
#define BIRON_AST_ATTR_H
#include <biron/ast.h>
#include <biron/util/string.h>

namespace Biron {

struct AstAttr : AstNode {
	static inline constexpr const auto KIND = Kind::ATTR;
	enum class Kind { BOOL, INT, STRING };
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

struct AstBoolAttr : AstAttr {
	static inline constexpr const auto KIND = Kind::BOOL;
	enum class Kind { USED, INLINE, ALIASABLE, REDZONE, EXPORT };
	constexpr AstBoolAttr(Kind kind, Bool value, Range range)
		: AstAttr{KIND, range}
		, m_kind{kind}
		, m_value{value}
	{
	}
	[[nodiscard]] constexpr Bool is_kind(Kind kind) const noexcept {
		return m_kind == kind;
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	constexpr Bool value() const noexcept { return m_value; }
private:
	Kind m_kind;
	Bool m_value;
};

struct AstIntAttr : AstAttr {
	static inline constexpr const auto KIND = Kind::INT;
	enum class Kind { ALIGN, ALIGNSTACK };
	constexpr AstIntAttr(Kind kind, Uint64 value, Range range)
		: AstAttr{KIND, range}
		, m_kind{kind}
		, m_value{value}
	{
	}
	[[nodiscard]] constexpr Bool is_kind(Kind kind) const noexcept {
		return m_kind == kind;
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	constexpr Uint64 value() const noexcept { return m_value; }
private:
	Kind   m_kind;
	Uint64 m_value;
};

struct AstStringAttr : AstAttr {
	static inline constexpr const auto KIND = Kind::STRING;
	enum class Kind { SECTION };
	constexpr AstStringAttr(Kind kind, StringView value, Range range)
		: AstAttr{KIND, range}
		, m_kind{kind}
		, m_value{value}
	{
	}
	[[nodiscard]] constexpr Bool is_kind(Kind kind) const noexcept {
		return m_kind == kind;
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	constexpr StringView value() const noexcept { return m_value; }
private:
	Kind       m_kind;
	StringView m_value;
};

} // namespace Biron

#endif // BIRON_AST_ATTR_H