#include <biron/ast_const.h>

namespace Biron {

AstConst::AstConst(AstConst&& other) noexcept
	: m_range{exchange(other.m_range, {0, 0})}
	, m_kind{exchange(other.m_kind, Kind::NONE)}
{
	switch (m_kind) {
	case Kind::NONE:
		break;
	case Kind::U8:  case Kind::U16:
	case Kind::U32: case Kind::U64: 
		m_as_uint = exchange(other.m_as_uint, 0);
		break;
	case Kind::S8:  case Kind::S16:
	case Kind::S32: case Kind::S64:
		m_as_sint = exchange(other.m_as_sint, 0);
		break;
	case Kind::B8:  case Kind::B16:
	case Kind::B32: case Kind::B64:
		m_as_bool = exchange(other.m_as_bool, false);
		break;
	case Kind::F32:
		m_as_f32 = exchange(other.m_as_f32, 0.0f);
		break;
	case Kind::F64:
		m_as_f64 = exchange(other.m_as_f64, 0.0);
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
	switch (m_kind) {
	case Kind::NONE:  return AstConst { range() };
	case Kind::U8:    return AstConst { range(), kind(), m_as_uint };
	case Kind::U16:   return AstConst { range(), kind(), m_as_uint };
	case Kind::U32:   return AstConst { range(), kind(), m_as_uint };
	case Kind::U64:   return AstConst { range(), kind(), m_as_uint };
	case Kind::S8:    return AstConst { range(), kind(), m_as_sint };
	case Kind::S16:   return AstConst { range(), kind(), m_as_sint };
	case Kind::S32:   return AstConst { range(), kind(), m_as_sint };
	case Kind::S64:   return AstConst { range(), kind(), m_as_sint };
	case Kind::B8:    return AstConst { range(), kind(), m_as_bool };
	case Kind::B16:   return AstConst { range(), kind(), m_as_bool };
	case Kind::B32:   return AstConst { range(), kind(), m_as_bool };
	case Kind::B64:   return AstConst { range(), kind(), m_as_bool };
	case Kind::F32:   return AstConst { range(), m_as_f32  };
	case Kind::F64:   return AstConst { range(), m_as_f64  };
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
}

} // namespace Biron