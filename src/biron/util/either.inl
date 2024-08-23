#ifndef BIRON_EITHER_INL
#define BIRON_EITHER_INL
#include <biron/util/traits/is_constructible.inl>

#include <biron/util/move.inl>
#include <biron/util/exchange.inl>
#include <biron/util/assert.inl>
#include <biron/util/new.inl>

namespace Biron {

template<MoveConstructible LHS, MoveConstructible RHS>
struct Either {
	constexpr Either() noexcept : m_as_nat{}, m_kind{KIND_NIL} {}
	constexpr Either(LHS&& lhs) noexcept : m_as_lhs{move(lhs)}, m_kind{KIND_LHS} {}
	constexpr Either(RHS&& rhs) noexcept : m_as_rhs{move(rhs)}, m_kind{KIND_RHS} {}
	constexpr Either(Either&& other) noexcept
		: m_kind{exchange(other.m_kind, KIND_NIL)}
	{
		/**/ if (is_lhs()) new (&m_as_lhs, Nat{}) LHS{move(other.m_as_lhs)};
		else if (is_rhs()) new (&m_as_rhs, Nat{}) RHS{move(other.m_as_rhs)};
		other.reset();
	}
	constexpr Either(const Either& other) noexcept
		requires CopyConstructible<LHS> && CopyConstructible<RHS>
		: m_kind{other.m_kind}
	{
		/**/ if (other.is_lhs()) new (&m_as_lhs, Nat{}) LHS{other.lhs()};
		else if (other.is_rhs()) new (&m_as_rhs, Nat{}) RHS{other.rhs()};
	}
	constexpr Either(const LHS& lhs) noexcept
		requires CopyConstructible<LHS>
		: m_as_lhs{lhs}
		, m_kind{KIND_LHS}
	{

	}
	constexpr Either(const RHS& rhs) noexcept
		requires CopyConstructible<RHS>
		: m_as_lhs{rhs}
		, m_kind{KIND_RHS}
	{
		
	}
	~Either() noexcept { reset(); }
	constexpr Either& operator=(LHS&& lhs) noexcept {
		return *new (drop(), Nat{}) Either{move(lhs)};
	}
	constexpr Either& operator=(RHS&& rhs) noexcept {
		return *new (drop(), Nat{}) Either{move(rhs)};
	}
	constexpr Either& operator=(Either&& other) noexcept {
		return *new (drop(), Nat{}) Either{move(other)};
	}
	constexpr Either& operator=(const LHS&) noexcept = delete;
	constexpr Either& operator=(const RHS&) noexcept = delete;
	constexpr Either& operator=(const Either&) noexcept = delete;
	[[nodiscard]] constexpr auto is_lhs() const noexcept { return m_kind == KIND_LHS; }
	[[nodiscard]] constexpr auto is_rhs() const noexcept { return m_kind == KIND_RHS; }
	[[nodiscard]] constexpr LHS& lhs() noexcept { BIRON_ASSERT(is_lhs()); return m_as_lhs; }
	[[nodiscard]] constexpr RHS& rhs() noexcept { BIRON_ASSERT(is_rhs()); return m_as_rhs; }
	[[nodiscard]] constexpr const LHS& lhs() const noexcept { BIRON_ASSERT(is_lhs()); return m_as_lhs; }
	[[nodiscard]] constexpr const RHS& rhs() const noexcept { BIRON_ASSERT(is_rhs()); return m_as_rhs; }
	template<typename... Ts>
	[[nodiscard]] constexpr LHS& emplace_lhs(Ts&&... args) noexcept {
		return (new(drop(), Nat{}) Either{LHS{forward<Ts>(args)...}})->lhs();
	}
	template<typename... Ts>
	[[nodiscard]] constexpr RHS& emplace_rhs(Ts&&... args) noexcept {
		return (new(drop(), Nat{}) Either{RHS{forward<Ts>(args)...}})->rhs();
	}
	[[nodiscard]] constexpr Bool operator==(const Either& other) const noexcept {
		if (other.m_kind != m_kind) {
			return false;
		}
		if (is_lhs()) {
			return other.lhs() == lhs();
		} else if (is_rhs()) {
			return other.rhs() == rhs();
		}
		return false;
	}
	void reset() noexcept { drop(); }
private:
	Either* drop() noexcept {
		/**/ if (is_lhs()) lhs().~LHS();
		else if (is_rhs()) rhs().~RHS();
		m_kind = KIND_NIL;
		return this;
	}
	union {
		Nat m_as_nat;
		[[no_unique_address]] LHS m_as_lhs;
		[[no_unique_address]] RHS m_as_rhs;
	};
	enum : char { KIND_LHS, KIND_RHS, KIND_NIL } m_kind;
};

} // namespace Biron

#endif // BIRON_EITHER_INL