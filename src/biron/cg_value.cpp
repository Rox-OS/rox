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

	// BuildGEP uses the struct type for how it will load but when indexing via
	// a pointer we must first dereference it. Likewise, when indexing through a
	// pointer we actually have to load the pointer.

	auto is_ptr = type->is_pointer();
	auto gep = cg.llvm.BuildGEP2(cg.builder,
	                             is_ptr ? type->deref()->ref(cg) : type->ref(cg),
	                             is_ptr ? load(cg)->ref() : ref(),
	                             indices + is_ptr,
	                             countof(indices) - is_ptr,
	                             "at");

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
	                             "at");
	if (!gep) {
		return None{};
	}

	// When working with arrays the base type is in 0 and no other indices are
	// actually valid.
	auto k = type->is_array() ? 0 : i;
	return CgAddr { type->at(k)->addrof(cg), gep };
}

Maybe<CgValue> CgValue::zero(CgType* type, Cg& cg) noexcept {
	auto& llvm = cg.llvm;
	switch (type->kind()) {
	case CgType::Kind::U8:  case CgType::Kind::S8:
	case CgType::Kind::U16: case CgType::Kind::S16:
	case CgType::Kind::U32: case CgType::Kind::S32:
	case CgType::Kind::U64: case CgType::Kind::S64:
		{
			auto value = llvm.ConstInt(type->ref(cg), 0, false);
			auto name = StringView(".ZeroInt");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::B8:
	case CgType::Kind::B16:
	case CgType::Kind::B32:
	case CgType::Kind::B64:
		{
			auto value = llvm.ConstInt(type->ref(cg), 0, false);
			auto name = StringView(".ZeroBool");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::POINTER:
		{
			auto value = llvm.ConstPointerNull(type->ref(cg));
			auto name = StringView(".ZeroPtr");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::STRING:
		[[fallthrough]];
	case CgType::Kind::SLICE:
		{
			LLVM::ValueRef values[] = {
				zero(type->at(0), cg)->ref(),
				zero(type->at(1), cg)->ref(),
			};
			auto value = llvm.ConstStructInContext(cg.context,
			                                       values,
			                                       countof(values),
			                                       false);
			StringView name;
			if (type->kind() == CgType::Kind::STRING) {
				name = ".ZeroString";
			} else {
				name = ".ZeroSlice";
			}
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
		break;
	case CgType::Kind::ARRAY:
		{
			Array<LLVM::ValueRef> zeros{cg.allocator};
			auto zero_elem = zero(type->deref(), cg);
			if (!zero_elem) {
				return None{};
			}
			if (!zeros.reserve(type->length())) {
				return None{};
			}
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				if (!zeros.push_back(zero_elem->ref())) {
					return None{};
				}
			}
			auto value = llvm.ConstArray2(type->deref()->ref(cg),
			                              zeros.data(),
			                              zeros.length());
			auto name = StringView(".ZeroArray");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::PADDING:
		{
			// Use type->at(0) to obtain the nested array and make a zero initialized
			// value of that type.
			LLVM::ValueRef values[] = {
				zero(type->at(0), cg)->ref()
			};
			// Then construct a constant structure with that zero initialized array
			// to produce the zero initialized padding.
			auto value = llvm.ConstNamedStruct(type->ref(cg), values, countof(values));
			auto name = StringView(".ZeroPad");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::TUPLE:
		{
			Array<LLVM::ValueRef> zeros{cg.allocator};
			if (!zeros.reserve(type->length())) {
				return None{};
			}
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				auto zero_elem = zero(type->at(i), cg);
				if (!zero_elem || !zeros.push_back(zero_elem->ref())) {
					return None{};
				}
			}
			auto value = llvm.ConstStructInContext(cg.context,
				                                     zeros.data(),
				                                     zeros.length(),
				                                     false);
			StringView name = ".ZeroTuple";
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
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
		{
			auto value = llvm.ConstPointerNull(type->ref(cg));
			auto name = StringView(".ZeroFn");
			llvm.SetValueName2(value, name.data(), name.length());
			return CgValue { type, value };
		}
	case CgType::Kind::VA:
		BIRON_ASSERT(!"Cannot generate zero value for '...'");
		return None{};
	}
	return None{};
}

} // namespace Biron