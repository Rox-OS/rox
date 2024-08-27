#ifndef BIRON_CG_VALUE_H
#define BIRON_CG_VALUE_H
#include <biron/llvm.h>
#include <biron/util/string.inl>

namespace Biron {

struct Cg;
struct CgType;
struct CgValue;

struct CgAddr {
	CgAddr(CgType *const type, LLVM::ValueRef ref) noexcept;

	Maybe<CgAddr> at(Cg& cg, Ulen index) const noexcept;
	Maybe<CgAddr> at(Cg& cg, const CgValue& index) const noexcept;

	Maybe<CgValue> load(Cg& cg) const noexcept;
	Bool store(Cg& cg, const CgValue& value) const noexcept;

	[[nodiscard]] constexpr CgType* type() const noexcept { return m_type; }
	[[nodiscard]] constexpr LLVM::ValueRef ref() const noexcept { return m_ref; }

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

	[[nodiscard]] constexpr LLVM::ValueRef ref() const noexcept { return m_ref; }

	// Construct a zero-value of the given type.
	static Maybe<CgValue> zero(CgType* type, Cg& cg) noexcept;

	[[nodiscard]] constexpr CgType* type() const noexcept { return m_type; }

	[[nodiscard]] CgAddr to_addr() const noexcept {
		return CgAddr { m_type, m_ref };
	}

private:
	CgType* m_type;
	LLVM::ValueRef m_ref;
};

struct CgVar {
	constexpr CgVar(StringView name, CgAddr&& addr) noexcept
		: m_name{name}
		, m_addr{move(addr)}
	{
	}
	[[nodiscard]] constexpr StringView name() const noexcept {
		return m_name;
	}
	[[nodiscard]] constexpr const CgAddr& addr() const noexcept {
		return m_addr;
	}
private:
	StringView m_name;
	CgAddr     m_addr;
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