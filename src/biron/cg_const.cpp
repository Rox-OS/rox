#include <biron/ast_const.h>
#include <biron/ast_type.h>

#include <biron/cg_value.h>
#include <biron/cg.h>

#include <biron/llvm.h>

namespace Biron {

Maybe<CgValue> AstConst::codegen(Cg& cg) const noexcept {
	ScratchAllocator scratch{cg.allocator};
	switch (kind()) {
	case Kind::NONE:
		return None{};
	case Kind::U8:
		return CgValue { cg.types.u8(), cg.llvm.ConstInt(cg.types.u8()->ref(), m_as_uint, false) };
	case Kind::U16:
		return CgValue { cg.types.u16(), cg.llvm.ConstInt(cg.types.u16()->ref(), m_as_uint, false) };
	case Kind::U32:
		return CgValue { cg.types.u32(), cg.llvm.ConstInt(cg.types.u32()->ref(), m_as_uint, false) };
	case Kind::U64:
		return CgValue { cg.types.u64(), cg.llvm.ConstInt(cg.types.u64()->ref(), m_as_uint, false) };
	case Kind::S8:
		return CgValue { cg.types.u8(), cg.llvm.ConstInt(cg.types.u8()->ref(), m_as_uint, false) };
	case Kind::S16:
		return CgValue { cg.types.s16(), cg.llvm.ConstInt(cg.types.s16()->ref(), m_as_uint, true) };
	case Kind::S32:
		return CgValue { cg.types.s32(), cg.llvm.ConstInt(cg.types.s32()->ref(), m_as_uint, true) };
	case Kind::S64:
		return CgValue { cg.types.s64(), cg.llvm.ConstInt(cg.types.s64()->ref(), m_as_uint, true) };
	case Kind::B8:
		return CgValue { cg.types.b8(), cg.llvm.ConstInt(cg.types.b8()->ref(), m_as_bool ? 1 : 0, false) };
	case Kind::B16:
		return CgValue { cg.types.b8(), cg.llvm.ConstInt(cg.types.b16()->ref(), m_as_bool ? 1 : 0, false) };
	case Kind::B32:
		return CgValue { cg.types.b8(), cg.llvm.ConstInt(cg.types.b32()->ref(), m_as_bool ? 1 : 0, false) };
	case Kind::B64:
		return CgValue { cg.types.b8(), cg.llvm.ConstInt(cg.types.b64()->ref(), m_as_bool ? 1 : 0, false) };
	case Kind::F32:
		return CgValue { cg.types.f32(), cg.llvm.ConstReal(cg.types.f32()->ref(), as_f32()) };
	case Kind::F64:
		return CgValue { cg.types.f64(), cg.llvm.ConstReal(cg.types.f64()->ref(), as_f64()) };
	case Kind::TUPLE:
		{
			// Generate constant CgValues for each tuple element.
			Array<CgValue> values{cg.allocator};
			if (!values.reserve(as_tuple().length())) {
				return None{};
			}
			for (const auto& elem : as_tuple()) {
				auto value = elem.codegen(cg);
				if (!value || !values.push_back(move(*value))) {
					return None{};
				}
			}
			// We generate a type even though ConstStructInContext does not need one,
			// this is just so we know where to insert padding and zeroinitialize it
			// and because every CgValue needs a pointer to a CgType.
			Array<CgType*> types{cg.allocator};
			if (!types.reserve(values.length())) {
				return None{};
			}
			for (const auto& value : values) {
				if (!types.push_back(value.type())) {
					return None{};
				}
			}
			auto type = cg.types.make(CgType::TupleInfo { move(types) });
			if (!type) {
				return None{};
			}
			// Walk the type declaration to know where to make padding zeroinitializer
			// and where to put our actual constant values.
			Array<LLVM::ValueRef> consts{scratch};
			Ulen j = 0;
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				auto field_type = type->at(i);
				if (field_type->is_padding()) {
					auto zero = CgValue::zero(field_type, cg);
					if (!zero || !consts.push_back(zero->ref())) {
						return None{};
					}
					continue;
				}
				if (!consts.push_back(values[j++].ref())) {
					return None{};
				}
			}
			auto value = cg.llvm.ConstStructInContext(cg.context,
			                                          consts.data(),
			                                          consts.length(),
			                                          false);
			if (!value) {
				return None{};
			}
			return CgValue { type, value };
		}
		break;
	case Kind::ARRAY:
		{
			// Generate constant CgValues for each array element.
			Array<CgValue> values{cg.allocator};
			if (!values.reserve(m_as_array.elems.length())) {
				return None{};
			}
			for (const auto& elem : m_as_array.elems) {
				auto value = elem.codegen(cg);
				if (!value) {
					return None{};
				}
				// if (*value->type() != *type) {
				// 	value->type()->dump(false);
				// 	fprintf(stderr, " != ");
				// 	type->dump();
				// 	// The element value type does not match the base type of the array.
				// 	// return None{};
				// }
				if (!values.push_back(move(*value))) {
					return None{};
				}
			}
			Array<LLVM::ValueRef> consts{scratch};
			if (!consts.reserve(values.length())) {
				return None{};
			}
			for (const auto& value : values) {
				if (!consts.push_back(value.ref())) {
					return None{};
				}
			}
			auto base = m_as_array.type->codegen(cg);
			if (!base) {
				return None{};
			}
			auto value = cg.llvm.ConstArray2(base->ref(),
			                                 consts.data(),
			                                 consts.length());
			if (!value) {
				return None{};
			}

			return CgValue { base, value };
		}
		break;
	case Kind::STRING:
		// TODO(dweiler): implement
		return None{};
	}
	return None{};
}

} // namespace Biron