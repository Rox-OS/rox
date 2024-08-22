#include <biron/ast_const.h>

namespace Biron {

AstConst::AstConst(AstConst&& other) noexcept
	: m_range{exchange(other.m_range, {0, 0})}
	, m_kind{exchange(other.m_kind, Kind::NONE)}
{
	switch (m_kind) {
	case Kind::NONE:
		break;
	case Kind::U8:
		m_as_u8  = exchange(other.m_as_u8, 0);
		break;
	case Kind::U16:
		m_as_u16 = exchange(other.m_as_u16, 0);
		break;
	case Kind::U32:
		m_as_u32 = exchange(other.m_as_u32, 0);
		break;
	case Kind::U64:
		m_as_u64 = exchange(other.m_as_u64, 0);
		break;
	case Kind::S8:
		m_as_s8  = exchange(other.m_as_s8, 0);
		break;
	case Kind::S16:
		m_as_s16 = exchange(other.m_as_s16, 0);
		break;
	case Kind::S32:
		m_as_s32 = exchange(other.m_as_s32, 0);
		break;
	case Kind::S64:
		m_as_s64 = exchange(other.m_as_s64, 0);
		break;
	case Kind::FIELD:
		m_as_field = exchange(other.m_as_field, {});
		break;
	case Kind::TUPLE: 
		new (&m_as_tuple, Nat{}) Array<AstConst>{move(other.m_as_tuple)};
		other.drop();
		break;
	case Kind::STRING:
		new (&m_as_string, Nat{}) StringView{move(other.m_as_string)};
		break;
	}
}

} // namespace Biron