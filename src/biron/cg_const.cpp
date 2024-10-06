#include <biron/ast_const.h>
#include <biron/ast_expr.h>
#include <biron/ast_type.h>

#include <biron/cg_value.h>
#include <biron/cg.h>

#include <biron/llvm.h>

namespace Biron {

Maybe<CgValue> AstConst::codegen(Cg& cg, CgType* type) const noexcept {
	// TODO(dweiler): Check that type matches the AstConst type.
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
			Array<CgValue> values{*cg.scratch};
			Maybe<Array<ConstField>> fields;
			auto& tuple = as_tuple();
			if (!values.reserve(tuple.values.length())) {
				return cg.oom();
			}
			Ulen i = 0;
			for (const auto& elem : tuple.values) {
				auto value = elem.codegen(cg, type->at(i));
				if (!value) {
					return None{};
				}
				if (!values.push_back(move(*value))) {
					return cg.oom();
				}
				i++;
			}
			if (tuple.fields) {
				auto& dst = fields.emplace(*cg.scratch);
				if (!dst.reserve(tuple.values.length())) {
					return cg.oom();
				}
				for (const auto& field : *tuple.fields) {
					// TODO init : field.init->codegen(cg, type->at(i))
					if (!dst.emplace_back(field.name, None{})) {
						return cg.oom();
					}
				}
			}
			// Walk the type declaration to know where to make padding zeroinitializer
			// and where to put our actual constant values.
			Array<LLVM::ValueRef> consts{*cg.scratch};
			Ulen j = 0;
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				auto field_type = type->at(i);
				if (field_type->is_padding()) {
					auto zero = CgValue::zero(field_type, cg);
					if (!zero) {
						return None{};
					}
					if (!consts.push_back(zero->ref())) {
						return cg.oom();
					}
					continue;
				}
				if (j >= values.length()) {
					// Zero initialize everything else not specified in the aggregate.
					auto zero = CgValue::zero(field_type, cg);
					if (!zero) {
						return None{};
					}
					if (!consts.push_back(zero->ref())) {
						return cg.oom();
					}
				} else if (!consts.push_back(values[j].ref())) {
					return cg.oom();
				}
				j++;
			}
			LLVM::ValueRef value = nullptr;
			if (cg.llvm.IsLiteralStruct(type->ref())) {
				value = cg.llvm.ConstStructInContext(cg.context,
				                                     consts.data(),
				                                     consts.length(),
				                                     false);
			} else {
				value = cg.llvm.ConstNamedStruct(type->ref(),
				                                 consts.data(),
				                                 consts.length());
			}
			return CgValue { type, value };
		}
		break;
	case Kind::ARRAY:
		{
			// Generate constant CgValues for each array element.
			Array<CgValue> values{*cg.scratch};
			if (!values.reserve(m_as_array.elems.length())) {
				return cg.oom();
			}
			CgType* array_type = type;
			if (auto typed = m_as_array.type) {
				auto type = typed->to_type<const AstArrayType>();
				auto extent = type->extent();
				if (extent->is_expr<const AstInferSizeExpr>()) {
					auto base = type->base()->codegen(cg, None{});
					if (!base) {
						return cg.oom();
					}
					array_type = cg.types.make(CgType::ArrayInfo {
						base,
						m_as_array.elems.length(),
						None{}
					});
				} else {
					array_type = type->codegen(cg, None{});
				}
			}
			if (!array_type) {
				return cg.oom();
			}
			for (const auto& elem : m_as_array.elems) {
				auto value = elem.codegen(cg, array_type->deref());
				if (!value) {
					return None{};
				}
				if (!values.push_back(move(*value))) {
					return cg.oom();
				}
			}
			Array<LLVM::ValueRef> consts{*cg.scratch};
			if (!consts.reserve(values.length())) {
				return cg.oom();
			}
			for (const auto& value : values) {
				if (!consts.push_back(value.ref())) {
					return cg.oom();
				}
			}
			auto value = cg.llvm.ConstArray2(array_type->deref()->ref(),
			                                 consts.data(),
			                                 consts.length());
			return CgValue { type, value };
		}
		break;
	case Kind::STRING:
		// TODO(dweiler): implement
		return None{};
	case Kind::UNTYPED_INT:
		if (type && type->is_integer()) {
			return CgValue { type, cg.llvm.ConstInt(type->ref(), m_as_uint, type->is_sint()) };
		}
		return cg.error(range(), "Untyped integer value must be typed");
	case Kind::UNTYPED_REAL:
		if (type && type->is_real()) {
			return CgValue { type, cg.llvm.ConstReal(type->ref(), m_as_f64) };
		}
		return cg.error(range(), "Untyped floating-point value must be typed");
	}
	return None{};
}

} // namespace Biron