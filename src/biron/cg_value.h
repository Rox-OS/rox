#ifndef BIRON_CG_VALUE_H
#define BIRON_CG_VALUE_H
#include <biron/llvm.h>
#include <biron/ast_const.h>
#include <biron/util/string.h>

namespace Biron {

struct Cg;
struct CgType;
struct CgValue;

struct AstNode;

struct CgAddr {
	CgAddr(CgType *const type, LLVM::ValueRef ref) noexcept;

	CgAddr at(Cg& cg, Ulen index) const noexcept;
	CgAddr at(Cg& cg, const CgValue& index) const noexcept;

	CgValue load(Cg& cg) const noexcept;
	Bool store(Cg& cg, const CgValue& value) const noexcept;
	Bool zero(Cg& cg) const noexcept;

	[[nodiscard]] constexpr LLVM::ValueRef ref() const noexcept { return m_ref; }
	[[nodiscard]] constexpr CgType* type() const noexcept { return m_type; }
	[[nodiscard]] CgValue to_value() const noexcept;

private:
	CgType* m_type;
	LLVM::ValueRef m_ref;
};

struct CgValue {
	constexpr CgValue(CgType *const type, LLVM::ValueRef ref) noexcept
		: m_type{type}
		, m_ref{ref}
	{
	}

	CgValue(CgValue&& other) noexcept
		: m_type{exchange(other.m_type, nullptr)}
		, m_ref{exchange(other.m_ref, nullptr)}
	{
	}

	constexpr CgValue(const CgValue& value) noexcept
		: m_type{value.m_type}
		, m_ref{value.m_ref}
	{
	}

	Maybe<CgValue> at(Cg& cg, Ulen i) const noexcept;

	// Construct a zero-value of the given type.
	static Maybe<CgValue> zero(CgType* type, Cg& cg) noexcept;

	[[nodiscard]] constexpr LLVM::ValueRef ref() const noexcept { return m_ref; }
	[[nodiscard]] constexpr CgType* type() const noexcept { return m_type; }
	[[nodiscard]] CgAddr to_addr() const noexcept;

private:
	CgType* m_type;
	LLVM::ValueRef m_ref;
};

inline CgAddr CgValue::to_addr() const noexcept {
	return CgAddr { m_type, m_ref };
}

inline CgValue CgAddr::to_value() const noexcept {
	return CgValue { m_type, m_ref };
}

struct CgVar {
	constexpr CgVar(const AstNode* node, StringView name, CgAddr&& addr) noexcept
		: m_node{node}
		, m_name{name}
		, m_addr{move(addr)}
	{
	}
	[[nodiscard]] constexpr const AstNode* node() const noexcept {
		return m_node;
	}
	[[nodiscard]] constexpr StringView name() const noexcept {
		return m_name;
	}
	[[nodiscard]] constexpr const CgAddr& addr() const noexcept {
		return m_addr;
	}
private:
	const AstNode* m_node;
	StringView     m_name;
	CgAddr         m_addr;
};

struct CgGlobal {
	CgGlobal(CgVar&& var, AstConst&& value) noexcept
		: m_var{move(var)}
		, m_value{move(value)}
	{
	}
	const CgVar& var() const noexcept { return m_var; }
	const AstConst& value() const noexcept { return m_value; }
private:
	CgVar m_var;
	AstConst m_value;
};

struct CgTypeDef {
	constexpr CgTypeDef(StringView name, CgType* type) noexcept
		: m_name{name}
		, m_type{type}
	{
	}
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] constexpr CgType* type() const noexcept { return m_type; }
private:
	StringView m_name;
	CgType*    m_type;
};

} // namespace Biron

#endif