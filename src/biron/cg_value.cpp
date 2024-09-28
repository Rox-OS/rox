#include <biron/cg_value.h>
#include <biron/cg.h>

namespace Biron {

// CgAddr
CgValue CgAddr::load(Cg& cg) const noexcept {
	auto type = m_type->deref();
	auto load = cg.llvm.BuildLoad2(cg.builder, type->ref(), m_ref, "");
	cg.llvm.SetAlignment(load, type->align());
	return CgValue { type, load };
}

Bool CgAddr::store(Cg& cg, const CgValue& value) const noexcept {
	auto type = value.type();
	// LLVM says not to generate store of structure or array types if we can avoid
	// it. This sounds like destructuring is a smarter approach.
	//
	// This could be simpler if we just do at(cg, i)->store(cg, value.at(cg, i))
	// but this would interleave the getelementptr + getvalueptr + store per field
	// of a tuple or element of the array. We do them all separately so it's easier
	// for LLVM to optimize and the IR is more readable.
	if (type->is_tuple() || type->is_array()) {
		// Generate a bunch of getelementptr
		Array<Maybe<CgAddr>> dst{*cg.scratch};
		if (!dst.resize(type->is_tuple() ? type->length() : type->extent())) {
			cg.oom();
			return false;
		}
		for (Ulen l = dst.length(), i = 0; i < l; i++) {
			dst[i] = at(cg, i);
		}
		// Generate a bunch of extractvalue
		Array<Maybe<CgValue>> extract{*cg.scratch};
		if (!extract.resize(dst.length())) {
			cg.oom();
			return false;
		}
		for (Ulen l = dst.length(), i = 0; i < l; i++) {
			extract[i] = value.at(cg, i);
		}
		// Generate a bunch of store of those extractedvalue into the getelementptr.
		for (Ulen l = dst.length(), i = 0; i < l; i++) {
			dst[i]->store(cg, *extract[i]);
		}
	} else {
		// Simple non-aggregate types we can just build a store for.
		auto store = cg.llvm.BuildStore(cg.builder, value.ref(), m_ref);
		cg.llvm.SetAlignment(store, value.type()->align());
	}
	return true;
}

Bool CgAddr::zero(Cg& cg) const noexcept {
	auto type = m_type->deref();
	// When the size is too large generate a call to memset instead
	if (type->size() > 4096) {
		auto src = cg.llvm.ConstInt(cg.types.u8()->ref(), 0, false);
		auto len = cg.llvm.ConstInt(cg.types.u64()->ref(), type->size(), false);
		cg.llvm.BuildMemSet(cg.builder, m_ref, src, len, type->align());
		return true;
	}
	auto zero = CgValue::zero(type, cg);
	return store(cg, *zero);
}

CgAddr CgAddr::at(Cg& cg, const CgValue& index) const noexcept {
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
		cg.llvm.ConstInt(cg.types.u32()->ref(), 0, false),
		index.ref(),
	};

	// BuildGEP uses the struct type for how it will load but when indexing via
	// a pointer we must first dereference it. Likewise, when indexing through a
	// pointer we actually have to load the pointer.

	auto is_ptr = type->is_pointer();
	auto gep = cg.llvm.BuildGEP2(cg.builder,
	                             is_ptr ? type->deref()->ref() : type->ref(),
	                             is_ptr ? load(cg).ref() : ref(),
	                             indices + is_ptr,
	                             countof(indices) - is_ptr,
	                             "at");

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

CgAddr CgAddr::at(Cg& cg, Ulen i) const noexcept {
	auto u32 = cg.types.u32();

	auto type = m_type->deref();
	LLVM::ValueRef indices[] = {
		cg.llvm.ConstInt(u32->ref(), 0, false),
		cg.llvm.ConstInt(u32->ref(), i, false),
	};

	auto is_ptr = type->is_pointer();
	auto gep = cg.llvm.BuildGEP2(cg.builder,
	                             is_ptr ? type->deref()->ref() : type->ref(),
	                             is_ptr ? load(cg).ref() : ref(),
	                             indices + is_ptr,
	                             countof(indices) - is_ptr,
	                             "");

	// When working with arrays the base type is in 0 and no other indices are
	// actually valid.
	auto k = type->is_array() ? 0 : i;
	return CgAddr { type->at(k)->addrof(cg), gep };
}

Maybe<CgValue> CgValue::at(Cg& cg, Ulen i) const noexcept {
	if (m_type->is_array() || m_type->is_string()) {
		auto value = cg.llvm.BuildExtractValue(cg.builder, m_ref, i, "");
		return CgValue { m_type->deref(), value };
	} else if (m_type->is_tuple()) {
		auto value = cg.llvm.BuildExtractValue(cg.builder, m_ref, i, "");
		return CgValue { m_type->at(i), value };
	}
	return None{};
}

Maybe<CgValue> CgValue::zero(CgType* type, Cg& cg) noexcept {
	auto value = cg.llvm.ConstNull(type->ref());
	return CgValue { type, value };
}

} // namespace Biron