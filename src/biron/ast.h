#ifndef BIRON_AST_H
#define BIRON_AST_H
#include <biron/util/numeric.inl>
#include <biron/util/traits/is_base_of.inl>
#include <biron/util/pool.h>

namespace Biron {

struct AstID {
	template<typename T>
	static Uint32 id() noexcept {
		static const Uint32 id = s_id++;
		return id;
	}
private:
	static inline Uint32 s_id;
};

struct AstNode {
	enum class Kind : Uint8 {
		TYPE, EXPR, STMT, FN, ASM, ATTR, MODULE, IMPORT, EFFECT
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

struct Ast {
	constexpr Ast(Allocator& allocator) noexcept
		: m_allocator{allocator}
		, m_caches{allocator}
	{
	}

	~Ast() noexcept {
		for (auto& cache : m_caches) if (cache) {
			for (auto node : *cache) {
				static_cast<AstNode*>(node)->~AstNode();
			}
		}
	}

	template<typename T, typename... Ts>
	[[nodiscard]] T* new_node(Ts&&... args) noexcept {
		static const Uint32 id = AstID::id<T>();
		if (id >= m_caches.length() && !m_caches.resize(id + 1)) {
			return nullptr;
		}
		auto& cache = m_caches[id];
		if (!cache) {
			cache = Cache { m_allocator, sizeof(T), 1024 };
		}
		auto addr = cache->allocate();
		if (!addr) {
			return nullptr;
		}
		return new (addr, Nat{}) T{forward<Ts>(args)...};
	}

private:
	Allocator&          m_allocator;
	Array<Maybe<Cache>> m_caches;
};

template<typename T>
struct AstRef {
	T& obj(Ast& ast) noexcept {
		return *ptr(ast);
	}
	const T& obj(const Ast& ast) const noexcept {
		return *ptr(ast);
	}
	T* ptr(Ast& ast) noexcept {
		return static_cast<T*>(ast.m_caches[AstID::id<T>()][m_index]);
	}
	const T* ptr(const Ast& ast) const noexcept {
		return static_cast<const T*>(ast.m_caches[AstID::id<T>()][m_index]);
	}
private:
	Uint32 m_index;
};

} // namespace Biron

#endif // BIRON_AST_H