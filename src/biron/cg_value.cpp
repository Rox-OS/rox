#include <biron/cg_value.h>
#include <biron/cg.h>

namespace Biron {

// CgAddr
Maybe<CgValue> CgAddr::load(Cg& cg) const noexcept {
	auto type = m_type->deref();
	auto load = cg.llvm.BuildLoad2(cg.builder, type->ref(cg), m_ref, "");
	return CgValue { type, load };
}

Bool CgAddr::store(Cg& cg, const CgValue& value) const noexcept {
	cg.llvm.BuildStore(cg.builder, value.ref(), m_ref);
	return true;
}

Maybe<CgAddr> CgAddr::at(Cg& cg, const CgValue& index) const noexcept {
	// Our CgAddr always has a pointer type. When indexing something through 'at'
	// at runtime we have to be careful because we're not generating an R-value.
	// We're generating an L-value because we may want to assign a result to the
	// thing being indexed (as an example).
	//
	// This is why we first need to take the type we have which will be a pointer
	// and dereference it. This will give us the non-pointer type which will be
	// the correct type had we actually dereferenced the expression, that is we
	// will have type of: array[index]
	//
	// Here we're only working with arrays or slices which are not aggregate types
	// so at(0) will yield the base type of the slice or array. We then need to
	// add the pointer back on that type since we're constructing an address to
	// that value at that index.
	//
	// Here's an example:
	//	let x = [2]Uint32{1, 2};
	//
	// The "storage" of 'x' has type *[2]Uint32 and internally we have a pointer
	// to 'x' in the compiler that we will call x_ptr. This is what CgAddr
	// represents. When we index x what we're actually doing is: &(*x_ptr)[index].
	//
	// This produces the following types
	// 	-  (*x_ptr)        is [2]Uint32
	// 	-  (*x_ptr)[index] is Uint32
	// 	- &(*x_ptr)[index] is *Uint32
	auto type = m_type->deref();
	LLVM::ValueRef indices[] = {
		cg.llvm.ConstInt(cg.types.u32()->ref(cg), 0, false),
		index.ref(),
	};

	auto is_ptr = type->is_pointer();
	auto gep = cg.llvm.BuildGEP2(cg.builder,
	                             type->ref(cg),
	                             is_ptr ? load(cg)->ref() : ref(),
	                             indices + is_ptr,
	                             countof(indices) - is_ptr,
	                             "");

	if (!gep) {
		return None{};
	}

	// Since we're working with slices or arrays here: type->at(0) produces the
	// base type of the slice or array and addrof adds the pointer back on that
	// type. To borrow the example above: [2]Uint32 -> Uint32 -> *Uint32.
	return CgAddr { type->at(0)->addrof(cg), gep };
}

CgAddr::CgAddr(CgType *const type, LLVM::ValueRef ref) noexcept
	: m_type{type}
	, m_ref{ref}
{
	BIRON_ASSERT(type->is_pointer() && "CgAddr constructed with a non-pointer");
}

Maybe<CgAddr> CgAddr::at(Cg& cg, Ulen i) const noexcept {
	auto u32 = cg.types.u32();
	LLVM::ValueRef indices[] = {
		cg.llvm.ConstInt(u32->ref(cg), 0, false),
		cg.llvm.ConstInt(u32->ref(cg), i, false),
	};

	auto type = m_type->deref();
	auto gep = cg.llvm.BuildGEP2(cg.builder,
	                             type->ref(cg),
	                             m_ref,
	                             indices,
	                             countof(indices),
	                             "");
	if (!gep) {
		return None{};
	}

	return CgAddr { type->at(i)->addrof(cg), gep };
}

Maybe<CgValue> CgValue::zero(CgType* type, Cg& cg) noexcept {
	auto& llvm = cg.llvm;
	switch (type->kind()) {
	case CgType::Kind::U8:  case CgType::Kind::S8:
	case CgType::Kind::U16: case CgType::Kind::S16:
	case CgType::Kind::U32: case CgType::Kind::S32:
	case CgType::Kind::U64: case CgType::Kind::S64:
		return CgValue { type, llvm.ConstInt(type->ref(cg), 0, false) };
	case CgType::Kind::B8:
	case CgType::Kind::B16:
	case CgType::Kind::B32:
	case CgType::Kind::B64:
		return CgValue { type, llvm.ConstInt(type->ref(cg), 0, false) };
	case CgType::Kind::POINTER:
		return CgValue { type, llvm.ConstPointerNull(type->ref(cg)) };
	case CgType::Kind::STRING:
		[[fallthrough]];
	case CgType::Kind::SLICE:
		{
			LLVM::ValueRef values[] = {
				llvm.ConstPointerNull(type->at(0)->ref(cg)),
				llvm.ConstInt(type->at(1)->ref(cg), 0, false),
			};
			return CgValue {
				type,
				llvm.ConstStructInContext(cg.context, values, 2, false),
			};
		}
		break;
	case CgType::Kind::ARRAY:
		{
			Array<LLVM::ValueRef> zeros{cg.allocator};
			auto zero_elem = zero(type->deref(), cg);
			if (!zero_elem) {
				return None{};
			}
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				if (!zeros.push_back(zero_elem->ref())) {
					return None{};
				}
			}
			return CgValue {
				type,
				cg.llvm.ConstArray2(type->deref()->ref(cg),
			                      zeros.data(),
			                      zeros.length())
			};
		}
	case CgType::Kind::PADDING:
		{
			// Use type->at(0) to obtain the nested array and make a zero initialized
			// value of that type.
			auto array = zero(type->at(0), cg);
			if (!array) {
				return None{};
			}
			// Then construct a constant structure with that zero initialized array
			// to produce the zero initialized padding.
			auto value = array->ref();
			return CgValue {
				type,
				cg.llvm.ConstNamedStruct(type->ref(cg), &value, 1)
			};
		}
	case CgType::Kind::TUPLE:
		[[fallthrough]];
	case CgType::Kind::STRUCT:
		{
			Array<LLVM::ValueRef> zeros{cg.allocator};
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				auto zero_elem = CgValue::zero(type->at(i), cg);
				if (!zero_elem || !zeros.push_back(zero_elem->ref())) {
					return None{};
				}
			}
			return CgValue {
				type,
				cg.llvm.ConstStructInContext(cg.context,
				                             zeros.data(),
				                             zeros.length(),
				                             false)
			};
		}
		break;
	case CgType::Kind::UNION:
		// LLVM does not have typed Unions. We implement them as an [N x i8] array
		// where N is the size of the largest field in the union. The alignment of
		// the union is set to the alignment of the largest field as well. This
		// makes it quite trivial to implement a constant zero union.
		if (auto array = cg.types.alloc(CgType::ArrayInfo { cg.types.u8(), type->size() })) {
			return zero(array, cg);
		}
		break;
	case CgType::Kind::FN:
		return CgValue {
			type,
			cg.llvm.ConstPointerNull(type->ref(cg))
		};
	case CgType::Kind::VA:
		BIRON_ASSERT(!"Cannot generate zero value for '...'");
		return None{};
	}
	return None{};
}

} // namespace Biron