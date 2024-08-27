#include <biron/ast_const.h>

namespace Biron {

AstConst::AstConst(AstConst&& other) noexcept
	: m_range{exchange(other.m_range, {0, 0})}
	, m_kind{exchange(other.m_kind, Kind::NONE)}
{
	switch (m_kind) {
	/****/ case Kind::NONE:
	break; case Kind::U8:   m_as_u8  = exchange(other.m_as_u8, 0);
	break; case Kind::U16:  m_as_u16 = exchange(other.m_as_u16, 0);
	break; case Kind::U32:  m_as_u32 = exchange(other.m_as_u32, 0);
	break; case Kind::U64:  m_as_u64 = exchange(other.m_as_u64, 0);
	break; case Kind::S8:   m_as_s8  = exchange(other.m_as_s8, 0);
	break; case Kind::S16:  m_as_s16 = exchange(other.m_as_s16, 0);
	break; case Kind::S32:  m_as_s32 = exchange(other.m_as_s32, 0);
	break; case Kind::S64:  m_as_s64 = exchange(other.m_as_s64, 0);
	break; case Kind::B8:   m_as_b8  = exchange(other.m_as_b8, false);
	break; case Kind::B16:  m_as_b16 = exchange(other.m_as_b16, false);
	break; case Kind::B32:  m_as_b32 = exchange(other.m_as_b32, false);
	break; case Kind::B64:  m_as_b64 = exchange(other.m_as_b64, false);
	break; case Kind::F32:  m_as_f32 = exchange(other.m_as_f32, 0.0f);
	break; case Kind::F64:  m_as_f64 = exchange(other.m_as_f64, 0.0);
	break;
	case Kind::TUPLE: 
		new (&m_as_tuple, Nat{}) Array<AstConst>{move(other.m_as_tuple)};
		other.drop();
		break;
	case Kind::STRING:
		new (&m_as_string, Nat{}) StringView{move(other.m_as_string)};
		break;
	case Kind::ARRAY:
		new (&m_as_array, Nat{}) ConstArray{move(other.m_as_array)};
		break;
	}
}

Maybe<AstConst> AstConst::copy() const noexcept {
	// GCC incorrectly sees the following call stack as possible:
	//
	//	AstConst::~AstConst()
	//		AstConst::drop()
	//			Array<AstConst>::~Array()
	//				Array<AstConst>::m_allocator <-- uninitialized
	//
	// Even though AstConst::drop() is only called when Kind::TUPLE which is also
	// the only time the Array<AstConst> is initialized. Just disable the warning
	// in here and restore it after.
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpragmas"
	#pragma GCC diagnostic ignored "-Wunknown-warning-option"
	#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	switch (m_kind) {
	case Kind::NONE:  return AstConst { range() };
	case Kind::U8:    return AstConst { range(), as_u8() };
	case Kind::U16:   return AstConst { range(), as_u16() };
	case Kind::U32:   return AstConst { range(), as_u32() };
	case Kind::U64:   return AstConst { range(), as_u64() };
	case Kind::S8:    return AstConst { range(), as_s8() };
	case Kind::S16:   return AstConst { range(), as_s16() };
	case Kind::S32:   return AstConst { range(), as_s32() };
	case Kind::S64:   return AstConst { range(), as_s64() };
	case Kind::B8:    return AstConst { range(), as_b8() };
	case Kind::B16:   return AstConst { range(), as_b16() };
	case Kind::B32:   return AstConst { range(), as_b32() };
	case Kind::B64:   return AstConst { range(), as_b64() };
	case Kind::F32:   return AstConst { range(), as_f32() };
	case Kind::F64:   return AstConst { range(), as_f64() };
	case Kind::TUPLE:
		{
			Array<AstConst> values{m_as_tuple.allocator()};
			if (!values.reserve(m_as_tuple.length())) {
				return None{};
			}
			for (const auto& it : m_as_tuple) {
				auto value = it.copy();
				if (!value || !values.push_back(move(*value))) {
					return None{};
				}
			}
			return AstConst { range(), move(values) };
		}
	case Kind::STRING: return AstConst { range(), as_string() };
	case Kind::ARRAY:
		{
			const auto& array = m_as_array.elems;
			Array<AstConst> values{array.allocator()};
			if (!values.reserve(array.length())) {
				return None{};
			}
			for (const auto& it : array) {
				auto value = it.copy();
				if (!value || !values.push_back(move(*value))) {
					return None{};
				}
			}
			return AstConst { range(), m_as_array.type, move(values) };
		}
	}
	return None{};
	#pragma GCC diagnostic pop
}

} // namespace Biron