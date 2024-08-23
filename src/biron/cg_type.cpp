#include <biron/cg.h>
#include <biron/ast_type.h>
#include <biron/ast_expr.h>
#include <biron/ast_const.h>

#include <biron/util/string.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgTypeCache> CgTypeCache::make(Allocator& allocator, Ulen capacity) {
	Cache cache{allocator, sizeof(CgType), capacity};
	CgType *const uints[4] = {
		alloc(cache, CgType::IntInfo { 1, 1, false }),
		alloc(cache, CgType::IntInfo { 2, 2, false }),
		alloc(cache, CgType::IntInfo { 4, 4, false }),
		alloc(cache, CgType::IntInfo { 8, 8, false }),
	};
	CgType *const sints[4] = {
		alloc(cache, CgType::IntInfo { 1, 1, true }),
		alloc(cache, CgType::IntInfo { 2, 2, true }),
		alloc(cache, CgType::IntInfo { 4, 4, true }),
		alloc(cache, CgType::IntInfo { 8, 8, true }),
	};
	CgType *const bools[4] = {
		alloc(cache, CgType::BoolInfo { 1, 1 }),
		alloc(cache, CgType::BoolInfo { 2, 2 }),
		alloc(cache, CgType::BoolInfo { 4, 4 }),
		alloc(cache, CgType::BoolInfo { 8, 8 }),
	};
	for (Ulen i = 0; i < 4; i++) {
		if (!uints[i] || !sints[i] || !bools[i]) {
			return None{};
		}
	}
	CgType *const ptr = alloc(cache, CgType::PtrInfo { nullptr, 8, 8 });
	CgType *const str = alloc(cache, CgType::StringInfo { ptr, uints[3] });
	if (!ptr || !str) {
		return None{};
	}
	// The empty tuple is the Unit type (i.e "void")
	CgType *const unit = alloc(cache, CgType::RecordInfo { true, {allocator} });
	if (!unit) {
		return None{};
	}
	CgType *const va = alloc(cache, CgType::VaInfo { });
	if (!va) {
		return None{};
	}
	return CgTypeCache {
		move(cache),
		uints,
		sints,
		bools,
		ptr,
		str,
		unit,
		va,
	};
}

Maybe<CgType> CgType::make(Cache&, IntInfo info) noexcept {
	static constexpr const Kind U[] = { Kind::U8, Kind::U16, Kind::U32, Kind::U64 };
	static constexpr const Kind S[] = { Kind::S8, Kind::S16, Kind::S32, Kind::S64 };

	// Work out the index from the integer log2 size.
	Ulen i = 0;
	for (Ulen j = info.size; j >>= 1; ++i);

	if (i >= countof(info.sign ? S : U)) {
		// Unsupported integer kind.
		return None{};
	}

	return CgType {
		info.sign ? S[i] : U[i],
		info.size,
		info.align,
		0,
		None{}
	};
}

Maybe<CgType> CgType::make(Cache& cache, PtrInfo info) noexcept {
	Maybe<Array<CgType*>> types;
	if (info.base) {
		if (!types.emplace(cache.allocator()).push_back(info.base)) {
			return None{};
		}
	}
	return CgType {
		Kind::POINTER,
		info.size,
		info.align,
		0,
		move(types)
	};
}

Maybe<CgType> CgType::make(Cache&, BoolInfo info) noexcept {
	static constexpr const Kind K[] = { Kind::B8, Kind::B16, Kind::B32, Kind::B64 };
	// Work out the index from the integer log2 size.
	Ulen i = 0;
	for (Ulen j = info.size; j >>= 1; ++i);

	if (i >= countof(K)) {
		// Unsupported boolean kind.
		return None{};
	}

	return CgType {
		K[i],
		info.size,
		info.align,
		0,
		None{}
	};
}

Maybe<CgType> CgType::make(Cache& cache, StringInfo info) noexcept {
	Array<CgType*> types{cache.allocator()};
	if (!types.push_back(info.ptr) || !types.push_back(info.len)) {
		return None{};
	}
	const auto size = info.ptr->size() + info.len->size();
	const auto align = max(info.ptr->align(), info.len->align());
	return CgType { Kind::STRING, size, align, 0, move(types) };
}

Maybe<CgType> CgType::make(Cache&, UnionInfo info) noexcept {
	Ulen size = 0;
	Ulen align = 0;
	// The size and alignment of a union type is the max size and alignment of
	// all nested types. We implement UNION as a simple [N x i8] which is later
	// bitcast to the appropriate types.
	for (Ulen l = info.types.length(), i = 0; i < l; i++) {
		const auto type = info.types[i];
		size = max(size, type->size());
		align = max(align, type->align());
	}
	return CgType {
		Kind::UNION,
		size,
		align,
		0,
		move(info.types)
	};
}

Maybe<CgType> CgType::make(Cache& cache, RecordInfo info) noexcept {
	// The size of a tuple and struct is the sum of all types as well as
	// padding needed between the fields for alignment. Likewise, the alignment
	// of a tuple and struct is the largest field alignment.
	//
	// We do not rely on LLVM here to introduce padding since we want padding
	// fields to be considered as accessible and addressible for the purposes
	// of deterministic hashing, memory comparisons, memory copying, and memory
	// zeroing. To make this possible we introduce our own custom padding type
	// between fields manually.
	//
	// The padding type is a special "scalar" type in the codegen which has an
	// alignment of one and the exact number of bytes needed to align the next
	// field. It's not an array type, it's a packed, nested structure type. The
	// reason we do it this way instead of using an array is because we can make
	// a module scoped %.PadN struct type declarations so the IR is easier to
	// read and because we save time within LLVM's ArrayType::get which is what
	// the LLVMArrayType2 function wraps. That function does a DenseMap lookup
	// each time an array type is constructed. This lookup is not done with a
	// struct type though and saves a lot of time inside LLVM.
	Array<CgType*> padded{cache.allocator()};
	Ulen offset = 0;
	Ulen alignment = 0;
	for (const auto& type : info.types) {
		if (!type->is_va()) {
			const auto align_mask = type->align() - 1;
			const auto aligned_offset = (offset + align_mask) & ~align_mask;
			if (auto padding = aligned_offset - offset) {
				auto pad = CgTypeCache::alloc(cache, PaddingInfo { padding });
				if (!pad || !padded.push_back(pad)) {
					return None{};
				}
			}
			offset = aligned_offset + type->size();
			alignment = max(alignment, type->align());
		}
		if (!padded.push_back(type)) {
			return None{};
		}
	}
	// We may need trailing padding at the end of the structure too.
	const auto align_mask = alignment - 1;
	const auto aligned_offset = (offset + align_mask) & ~align_mask;
	if (auto padding = aligned_offset - offset) {
		auto pad = CgTypeCache::alloc(cache, PaddingInfo { padding });
		if (!pad || !padded.push_back(pad)) {
			return None{};
		}
	}
	return CgType {
		info.tuple ? Kind::TUPLE : Kind::STRUCT,
		offset,
		alignment,
		0,
		move(padded)
	};
}

Maybe<CgType> CgType::make(Cache& cache, ArrayInfo info) noexcept {
	if (info.extent == 0) {
		return None{};
	}
	Array<CgType*> types{cache.allocator()};
	if (!types.push_back(info.base)) {
		return None{};
	}
	return CgType {
		Kind::ARRAY,
		info.base->size() * info.extent,
		info.base->align(),
		info.extent,
		move(types)
	};
}

Maybe<CgType> CgType::make(Cache& cache, SliceInfo info) noexcept {
	auto ptr = CgTypeCache::alloc(cache, PtrInfo { info.base, 8, 8 });
	auto len = CgTypeCache::alloc(cache, IntInfo { 8, 8, false });
	if (!ptr || !len) {
		return None{};
	}
	Array<CgType*> types{cache.allocator()};
	if (!types.push_back(ptr) || !types.push_back(len)) {
		return None{};
	}
	return CgType {
		Kind::SLICE,
		16,
		8,
		0,
		move(types)
	};
}

Maybe<CgType> CgType::make(Cache& cache, PaddingInfo info) noexcept {
	// Ensure we have a i8
	auto u8 = CgTypeCache::alloc(cache, IntInfo { 1, 1, false });
	if (!u8) {
		return None{};
	}
	// Using i8 make a [N x i8]
	auto array = CgTypeCache::alloc(cache, ArrayInfo { u8, info.padding });
	if (!array) {
		return None{};
	}
	Array<CgType*> types{cache.allocator()};
	if (!types.push_back(array)) {
		return None{};
	}
	return CgType {
		Kind::PADDING,
		info.padding,
		1,
		0,
		move(types)
	};
}

Maybe<CgType> CgType::make(Cache& cache, FnInfo info) noexcept {
	Array<CgType*> types{cache.allocator()};
	if (!types.push_back(info.args) || !types.push_back(info.rets)) {
		return None{};
	}
	return CgType {
		Kind::FN,
		0,
		0,
		0,
		move(types)
	};
}

Maybe<CgType> CgType::make(Cache&, VaInfo) noexcept {
	return CgType { Kind::VA, 0, 0, 0, None{} };
}

CgType* CgType::addrof(Cg& cg) noexcept {
	return cg.types.alloc(CgType::PtrInfo { this, 8, 8 });
}

LLVM::TypeRef CgType::ref(Cg& cg) noexcept {
	if (m_ref) {
		// We already have generated the LLVM::TypeRef
		return m_ref;
	}

	auto& llvm = cg.llvm;
	auto& context = cg.context;
	switch (m_kind) {
	case Kind::U8:
	case Kind::S8:
		m_ref = llvm.Int8TypeInContext(context);
		break;
	case Kind::U16:
	case Kind::S16:
		m_ref = llvm.Int16TypeInContext(context);
		break;
	case Kind::U32:
	case Kind::S32:
		m_ref = llvm.Int32TypeInContext(context);
		break;
	case Kind::U64:
	case Kind::S64:
		m_ref = llvm.Int64TypeInContext(context);
		break;
	case Kind::B8:
	case Kind::B16:
	case Kind::B32:
	case Kind::B64:
		m_ref = llvm.Int1TypeInContext(context);
		break;
	case Kind::POINTER:
		m_ref = llvm.PointerTypeInContext(context, 0);
		break;
	case Kind::SLICE:
		if (auto find = llvm.GetTypeByName2(context, ".Slice")) {
			m_ref = find;
		} else if (auto type = llvm.StructCreateNamed(context, ".Slice")) {
			LLVM::TypeRef types[2] = {
				cg.types.ptr()->ref(cg),
				cg.types.u64()->ref(cg),
			};
			llvm.StructSetBody(type, types, countof(types), false);
			m_ref = type;
		}
		break;
	case Kind::STRING:
		if (auto find = llvm.GetTypeByName2(context, ".String")) {
			m_ref = find;
		} else if (auto type = llvm.StructCreateNamed(context, ".String")) {
			LLVM::TypeRef types[2] = {
				cg.types.ptr()->ref(cg),
				cg.types.u64()->ref(cg),
			};
			llvm.StructSetBody(type, types, countof(types), false);
			m_ref = type;
		}
		break;
	case Kind::TUPLE:
		// An empty tuple is the void type.
		if (m_types->length() == 0) {
			m_ref = llvm.VoidTypeInContext(context);
			break;
		}
		[[fallthrough]];
	case Kind::STRUCT:
		{
			Array<LLVM::TypeRef> types{cg.allocator};
			for (const auto type : *m_types) {
				if (!types.push_back(type->ref(cg))) {
					return nullptr;
				}
			}
			m_ref = llvm.StructTypeInContext(cg.context,
			                                 types.data(),
			                                 types.length(),
			                                 false);
		}
		break;
	case Kind::ARRAY:
		m_ref = llvm.ArrayType2(at(0)->ref(cg), m_extent);
		break;
	case Kind::PADDING:
		{
			// Special "scalar" type that has the exact size of the padding needed
			// before the next aligned field of a structure or tuple. Within LLVM it
			// is implemented as a { [N x i8] } and has the name .Pad%d where %d is
			// the # of bytes of padding.
			StringBuilder name{cg.allocator};
			name.append(".Pad");
			name.append(m_size);
			name.append('\0');

			if (!name.valid()) {
				return nullptr;
			}

			// See if we can find a padding type already named this.
			auto type = llvm.GetTypeByName2(cg.context, name.data());
			if (type) {
				m_ref = type;
			} else {
				// We couldn't which means this is the first time generating it.
				auto type = llvm.StructCreateNamed(cg.context, name.data());
				if (!type) {
					return nullptr;
				}

				// Populate it with the [N x i8] padding array contained in at(0)
				auto array = at(0)->ref(cg);
		
				// The structure is "packed" even though the only thing contained within
				// it is a byte array which has alignment one. We mark it packed here
				// because the only operation that is performed on the data from the IR
				// size is a zeroinitializer and it's a good canonical form.
				llvm.StructSetBody(type, &array, 1, true);

				m_ref = type;
			}
		}
		break;
	case Kind::UNION:
		m_ref = llvm.ArrayType2(cg.types.u8()->ref(cg), m_size);
		break;
	case Kind::FN:
		{
			// When making a function type the nested type array should contain two
			// tuple types: (args) -> (rets)
			//
			// We do not generate a tuple for the args though, we prefer to expand the
			// nested types of the tuple directly into the function. This means the
			// padding we insert inbetween tuple fields as-if we want to use them as
			// structures needs to be omitted for ABI reasons. When generating the
			// rets we only generate a tuple if there is more than one return type,
			// otherwise we detuple it.
			Array<LLVM::TypeRef> types{cg.allocator};
			const auto& args = at(0);
			Bool has_va = false;
			for (Ulen l = args->length(), i = 0; i < l; i++) {
				auto arg = args->at(i);
				if (arg->is_padding()) {
					continue;
				}
				if (arg->is_va()) {
					has_va = true;
					break;
				}
				if (!types.push_back(arg->ref(cg))) {
					return nullptr;
				}
			}
			const auto& rets = at(1);
			if (rets->length() == 1) {
				// We do not generate a tuple for the return when there is only a single
				// return type in the tuple.
				m_ref = llvm.FunctionType(rets->at(0)->ref(cg), types.data(), types.length(), has_va);
			} else {
				m_ref = llvm.FunctionType(rets->ref(cg), types.data(), types.length(), has_va);
			}
		}
		break;
	case Kind::VA:
		BIRON_ASSERT(false && "Cannot generate an LLVM type for '...'");
		break;
	}
	return m_ref;
}

CgType* AstTupleType::codegen(Cg& cg) const noexcept {
	Array<CgType*> types{cg.allocator};
	for (Ulen l = m_elems.length(), i = 0; i < l; i++) {
		const auto& elem = m_elems[i];
		auto type = elem.type()->codegen(cg);
		if (!type || !types.push_back(type)) {
			return nullptr;
		}
	}
	return cg.types.alloc(CgType::RecordInfo { true, move(types) });
}

CgType* AstIdentType::codegen(Cg& cg) const noexcept {
	if (m_ident == "Uint8")  return cg.types.u8();
	if (m_ident == "Uint16") return cg.types.u16();
	if (m_ident == "Uint32") return cg.types.u32();
	if (m_ident == "Uint64") return cg.types.u64();
	if (m_ident == "Sint8")  return cg.types.s8();
	if (m_ident == "Sint16") return cg.types.s16();
	if (m_ident == "Sint32") return cg.types.s32();
	if (m_ident == "Sint64") return cg.types.s64();
	if (m_ident == "Bool8")  return cg.types.b8();
	if (m_ident == "Bool16") return cg.types.b16();
	if (m_ident == "Bool32") return cg.types.b32();
	if (m_ident == "Bool64") return cg.types.b64();
	if (m_ident == "String") return cg.types.str();
	// Search for the 'struct' definition for m_ident
	for (const auto& record : cg.structs) {
		if (record.name == m_ident) {
			return record.type;
		}
	}
	// TODO(dweiler): Symbol table for types
	return nullptr;
}

CgType* AstVarArgsType::codegen(Cg&) const noexcept {
	// There is nothing to codegen for a va.
	return nullptr;
}

CgType* AstPtrType::codegen(Cg& cg) const noexcept {
	auto base = m_type->codegen(cg);
	if (!base) {
		return nullptr;
	}
	return cg.types.alloc(CgType::PtrInfo { base, 8, 8 });
}

CgType* AstArrayType::codegen(Cg& cg) const noexcept {
	auto base = m_type->codegen(cg);
	if (!base) {
		return nullptr;
	}
	auto value = m_extent->eval();
	if (!value || !value->is_integral()) {
		// Not a compile time integer constant expression.
		return nullptr;
	}
	auto extent = value->to<Uint64>();
	if (!extent) {
		// Cannot cast integer constant expression to Uint64 extent
		return nullptr;
	}
	return cg.types.alloc(CgType::ArrayInfo { base, extent->as_u64() });
}

CgType* AstSliceType::codegen(Cg& cg) const noexcept {
	// TODO(dweiler): Implement
	auto base = m_type->codegen(cg);
	if (!base) {
		return nullptr;
	}
	return cg.types.alloc(CgType::SliceInfo { base });
}

} // namespace Biron