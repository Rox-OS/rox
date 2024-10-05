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
		new (&m_as_tuple, Nat{}) ConstTuple{move(other.m_as_tuple)};
		other.drop();
		break;
	case Kind::ARRAY:
		new (&m_as_array, Nat{}) ConstArray{move(other.m_as_array)};
		other.drop();
		break;
	case Kind::STRING:
		new (&m_as_string, Nat{}) StringView{move(other.m_as_string)};
		break;
	case Kind::UNTYPED_INT:
		m_as_uint = exchange(other.m_as_uint, 0);
		break;
	case Kind::UNTYPED_REAL:
		m_as_f64 = exchange(other.m_as_f64, 0.0);
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
			auto values = m_as_tuple.values.copy();
			auto fields = m_as_tuple.fields.copy();
			if (!values || !fields) {
				return None{};
			}
			return AstConst {
				range(),
				ConstTuple { m_as_tuple.type, move(*values), move(*fields) }
			};
		}
	case Kind::STRING:
		return AstConst { range(), as_string() };
	case Kind::ARRAY:
		{
			auto elems = m_as_array.elems.copy();
			if (!elems) {
				return None{};
			}
			return AstConst {
				range(),
				ConstArray { m_as_array.type, move(*elems) }
			};
		}
	case Kind::UNTYPED_INT:
		return AstConst { range(), AstConst::UntypedInt { m_as_uint } };
	case Kind::UNTYPED_REAL:
		return AstConst { range(), AstConst::UntypedReal { m_as_f64 } };
	}
	return None{};
}

void AstConst::drop() noexcept {
	// GCC is actually quite silly here.
	#if defined(BIRON_COMPILER_GCC)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	#endif
	if (m_kind == Kind::TUPLE) {
		m_as_tuple.~ConstTuple();
	} else if (m_kind == Kind::ARRAY) {
		m_as_array.~ConstArray();
	}
	#if defined(BIRON_COMPILER_GCC)
	#pragma GCC diagnostic pop
	#endif
}

Maybe<ConstField> ConstField::copy() const noexcept {
	auto copy_name = name.copy();
	auto copy_init = init.copy();
	if (!copy_name || !copy_init) {
		return None{};
	}
	return ConstField { move(copy_name), move(copy_init) };
}

} // namespace Biron