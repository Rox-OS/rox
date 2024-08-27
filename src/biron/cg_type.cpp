#include <biron/cg.h>
#include <biron/ast_type.h>
#include <biron/ast_expr.h>
#include <biron/ast_const.h>

#include <biron/util/string.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgTypeCache> CgTypeCache::make(Allocator& allocator, LLVM& llvm, LLVM::ContextRef context, Ulen capacity) {
	Cache cache{allocator, sizeof(CgType), capacity};
	// We will construct a "bootstrapping" CgTypeCache which will be used to
	// construct some builtin types which are always expected to exist.
	CgTypeCache bootstrap{move(cache), llvm, context};
	CgType* (&builtin)[countof(bootstrap.m_builtin)] = bootstrap.m_builtin;
	builtin[0]  = bootstrap.make(CgType::IntInfo    { 1, 1, false });
	builtin[1]  = bootstrap.make(CgType::IntInfo    { 2, 2, false });
	builtin[2]  = bootstrap.make(CgType::IntInfo    { 4, 4, false });
	builtin[3]  = bootstrap.make(CgType::IntInfo    { 8, 8, false });
	builtin[4]  = bootstrap.make(CgType::IntInfo    { 1, 1, true  });
	builtin[5]  = bootstrap.make(CgType::IntInfo    { 2, 2, true  });
	builtin[6]  = bootstrap.make(CgType::IntInfo    { 4, 4, true  });
	builtin[7]  = bootstrap.make(CgType::IntInfo    { 8, 8, true  });
	builtin[8]  = bootstrap.make(CgType::BoolInfo   { 1, 1 });
	builtin[9]  = bootstrap.make(CgType::BoolInfo   { 2, 2 });
	builtin[10] = bootstrap.make(CgType::BoolInfo   { 4, 4 });
	builtin[11] = bootstrap.make(CgType::BoolInfo   { 8, 8 });
	builtin[12] = bootstrap.make(CgType::FltInfo    { 4, 4 }),
	builtin[13] = bootstrap.make(CgType::FltInfo    { 8, 8 }),
	builtin[14] = bootstrap.make(CgType::PtrInfo    { nullptr, 8, 8 });
	builtin[15] = bootstrap.make(CgType::StringInfo { });
	builtin[16] = bootstrap.make(CgType::TupleInfo  { { allocator } });
	builtin[17] = bootstrap.make(CgType::VaInfo     { });
	for (Ulen i = 0; i < countof(builtin); i++) {
		if (!builtin[i]) {
			return None{};
		}
	}
	return move(bootstrap);
}

void CgType::dump(StringBuilder& builder) const noexcept {
	switch (m_kind) {
	case Kind::U8:
		builder.append("Uint8");
		break;
	case Kind::U16:
		builder.append("Uint16");
		break;
	case Kind::U32:
		builder.append("Uint32");
		break;
	case Kind::U64:
		builder.append("Uint64");
		break;
	case Kind::S8:
		builder.append("Sint8");
		break;
	case Kind::S16:
		builder.append("Sint16");
		break;
	case Kind::S32:
		builder.append("Sint32");
		break;
	case Kind::S64:
		builder.append("Sint64");
		break;
	case Kind::B8:
		builder.append("Bool8");
		break;
	case Kind::B16:
		builder.append("Bool16");
		break;
	case Kind::B32:
		builder.append("Bool32");
		break;
	case Kind::B64:
		builder.append("Bool64");
		break;
	case Kind::F32:
		builder.append("Float32");
		break;
	case Kind::F64:
		builder.append("Float64");
		break;
	case Kind::STRING:
		builder.append("String");
		break;
	case Kind::POINTER:
		builder.append('*');
		if (auto base = at(0)) {
			base->dump(builder);
		}
		break;
	case Kind::SLICE:
		builder.append("[]");
		at(0)->dump(builder);
		break;
	case Kind::ARRAY:
		builder.append('[');
		builder.append(m_extent);
		builder.append(']');
		at(0)->dump(builder);
		break;
	case Kind::PADDING:
		builder.append(".Pad");
		builder.append(m_size);
		break;
	case Kind::UNION:
		// TODO
		break;
	case Kind::TUPLE:
		{
			builder.append('(');
			for (Ulen l = length(), i = 0; i < l; i++) {
				at(i)->dump(builder);
				if (i != l - 1) {
					builder.append(", ");
				}
			}
			builder.append(')');
		}
		break;
	case Kind::FN:
		{
			const auto& args = at(0)->types();
			const auto& rets = at(1)->types();
			builder.append("fn");
			builder.append('(');
			Bool f = true;
			for (const auto& arg : args) {
				if (!f) {
					builder.append(", ");
				}
				arg->dump(builder);
				f = false;
			}
			builder.append(')');
			builder.append(" -> ");
			builder.append('(');
			f = true;
			for (const auto& ret : rets) {
				if (!f) {
					builder.append(", ");
				}
				ret->dump(builder);
				f = false;
			}
			builder.append(')');
		}
		break;
	case Kind::VA:
		builder.append("...");
		break;
	}
}

CgType* CgType::addrof(Cg& cg) noexcept {
	return cg.types.make(CgType::PtrInfo { this, 8, 8 });
}

CgType* AstTupleType::codegen(Cg& cg) const noexcept {
	Array<CgType*> types{cg.allocator};
	if (!types.reserve(m_elems.length())) {
		return nullptr;
	}
	for (auto& elem : m_elems) {
		auto type = elem.type()->codegen(cg);
		if (!type || !types.push_back(type)) {
			return nullptr;
		}
	}
	return cg.types.make(CgType::TupleInfo { move(types) });
}

CgType* AstIdentType::codegen(Cg& cg) const noexcept {
	if (m_ident == "Uint8")   return cg.types.u8();
	if (m_ident == "Uint16")  return cg.types.u16();
	if (m_ident == "Uint32")  return cg.types.u32();
	if (m_ident == "Uint64")  return cg.types.u64();
	if (m_ident == "Sint8")   return cg.types.s8();
	if (m_ident == "Sint16")  return cg.types.s16();
	if (m_ident == "Sint32")  return cg.types.s32();
	if (m_ident == "Sint64")  return cg.types.s64();
	if (m_ident == "Bool8")   return cg.types.b8();
	if (m_ident == "Bool16")  return cg.types.b16();
	if (m_ident == "Bool32")  return cg.types.b32();
	if (m_ident == "Bool64")  return cg.types.b64();
	if (m_ident == "Float32") return cg.types.f32();
	if (m_ident == "Float64") return cg.types.f64();
	if (m_ident == "String")  return cg.types.str();
	if (m_ident == "Address") return cg.types.ptr();
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
	return cg.types.make(CgType::PtrInfo { base, 8, 8 });
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
	return cg.types.make(CgType::ArrayInfo { base, *extent });
}

CgType* AstSliceType::codegen(Cg& cg) const noexcept {
	auto base = m_type->codegen(cg);
	if (!base) {
		return nullptr;
	}
	return cg.types.make(CgType::SliceInfo { base });
}

CgType* AstFnType::codegen(Cg& cg) const noexcept {
	auto args = m_args->codegen(cg);
	if (!args) {
		return nullptr;
	}
	auto rets = m_rets->codegen(cg);
	if (!rets) {
		return nullptr;
	}
	return cg.types.make(CgType::FnInfo { args, rets });
}

CgType* CgTypeCache::make(CgType::IntInfo info) noexcept {
	LLVM::TypeRef ref = nullptr;
	CgType::Kind kind;
	switch (info.size) {
	case 8:
		ref = m_llvm.Int64TypeInContext(m_context);
		kind = info.sign ? CgType::Kind::S64 : CgType::Kind::U64;
		break;
	case 4:
		ref = m_llvm.Int32TypeInContext(m_context);
		kind = info.sign ? CgType::Kind::S32 : CgType::Kind::U32;
		break;
	case 2:
		ref = m_llvm.Int16TypeInContext(m_context);
		kind = info.sign ? CgType::Kind::S16 : CgType::Kind::U16;
		break;
	case 1:
		ref = m_llvm.Int8TypeInContext(m_context);
		kind = info.sign ? CgType::Kind::S8 : CgType::Kind::U8;
	}
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		kind,
		info.size,
		info.align,
		0,
		None{},
		ref
	};
}

CgType* CgTypeCache::make(CgType::FltInfo info) noexcept {
	LLVM::TypeRef ref = nullptr;
	CgType::Kind kind;
	switch (info.size) {
	case 8:
		ref = m_llvm.DoubleTypeInContext(m_context);
		kind = CgType::Kind::F64;
		break;
	case 4:
		ref = m_llvm.FloatTypeInContext(m_context);
		kind = CgType::Kind::F32;
		break;
	}
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		kind,
		info.size,
		info.align,
		0,
		None{},
		ref
	};
}

CgType* CgTypeCache::make(CgType::PtrInfo info) noexcept {
	Maybe<Array<CgType*>> types{m_cache.allocator()};
	if (info.base && !types.emplace(m_cache.allocator()).push_back(info.base)) {
		return nullptr;
	}
	auto ref = m_llvm.PointerTypeInContext(m_context, 0);
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::POINTER,
		info.size,
		info.align,
		0,
		move(types),
		ref,
	};
}

CgType* CgTypeCache::make(CgType::BoolInfo info) noexcept {
	auto ref = m_llvm.Int1TypeInContext(m_context);
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	// TODO(select: B8->)
	return new(dst, Nat{}) CgType {
		CgType::Kind::B32,
		info.size,
		info.align,
		0,
		None{},
		ref
	};
}

CgType* CgTypeCache::make(CgType::StringInfo) noexcept {
	LLVM::TypeRef ref = nullptr;
	if (auto find = m_llvm.GetTypeByName2(m_context, ".String")) {
		ref = find;
	} else if (auto type = m_llvm.StructCreateNamed(m_context, ".String")) {
		LLVM::TypeRef types[2] = {
			ptr()->ref(),
			u64()->ref(),
		};
		m_llvm.StructSetBody(type, types, countof(types), false);
		ref = type;
	} else {
		return nullptr;
	}
	Array<CgType*> types{m_cache.allocator()};
	if (!types.resize(2)) {
		return nullptr;
	}
	types[0] = ptr();
	types[1] = u64();
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::STRING,
		sum(ptr()->size(), u64()->size()),
		max(ptr()->align(), u64()->align()),
		0,
		move(types),
		ref,
	};
}

CgType* CgTypeCache::make(CgType::TupleInfo info) noexcept {
	Array<CgType*> padded{m_cache.allocator()};
	if (!padded.reserve(info.types.length())) {
		return nullptr;
	}
	Ulen offset = 0;
	Ulen alignment = 0;
	for (auto type : info.types) {
		if (!type->is_va()) {
			const auto align_mask = type->align() - 1;
			const auto aligned_offset = (offset + align_mask) & ~align_mask;
			if (auto padding = aligned_offset - offset) {
				auto pad = make(CgType::PaddingInfo { padding });
				if (!pad || !padded.push_back(pad)) {
					return nullptr;
				}
			}
			offset = sum(aligned_offset, type->size());
			alignment = max(alignment, type->align());
		}
		if (!padded.push_back(type)) {
			return nullptr;
		}
	}
	const auto align_mask = alignment - 1;
	const auto aligned_offset = (offset + align_mask) & ~align_mask;
	if (auto padding = aligned_offset - offset) {
		auto pad = make(CgType::PaddingInfo { padding });
		if (!pad || !padded.push_back(pad)) {
			return nullptr;
		}
	}

	LLVM::TypeRef ref = nullptr;
	if (padded.empty()) {
		ref = m_llvm.VoidTypeInContext(m_context);
	} else {
		ScratchAllocator scratch{m_cache.allocator()};
		Array<LLVM::TypeRef> types{scratch};
		if (!types.reserve(padded.length())) {
			return nullptr;
		}
		for (auto type : padded) {
			if (!types.push_back(type->ref())) {
				return nullptr;
			}
		}
		ref = m_llvm.StructTypeInContext(m_context, types.data(), types.length(), false);
	}
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::TUPLE,
		offset,
		alignment,
		0,
		move(padded),
		ref
	};
}

CgType* CgTypeCache::make(CgType::ArrayInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(info.base)) {
		return nullptr;
	}
	auto ref = m_llvm.ArrayType2(info.base->ref(), info.extent);
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::ARRAY,
		info.base->size() * info.extent,
		info.base->align(),
		info.extent,
		move(types),
		ref
	};
}

CgType* CgTypeCache::make(CgType::SliceInfo info) noexcept {
	LLVM::TypeRef ref = nullptr;
	if (auto find = m_llvm.GetTypeByName2(m_context, ".Slice")) {
		ref = find;
	} else if (auto type = m_llvm.StructCreateNamed(m_context, ".Slice")) {
		LLVM::TypeRef types[2] = {
			ptr()->ref(),
			u64()->ref(),
		};
		m_llvm.StructSetBody(type, types, countof(types), false);
		ref = type;
	} else {
		return nullptr;
	}
	Array<CgType*> types{m_cache.allocator()};
	if (!types.resize(2)) {
		return nullptr;
	}
	types[0] = info.base;
	types[1] = u64();
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::SLICE,
		sum(ptr()->size(), u64()->size()),
		max(ptr()->align(), u64()->align()),
		0,
		move(types),
		ref,
	};
}

CgType* CgTypeCache::make(CgType::PaddingInfo info) noexcept {
	StringBuilder name{m_cache.allocator()};
	name.append(".Pad");
	name.append(info.padding);
	name.append('\0');
	if (!name.valid()) {
		return nullptr;
	}
	auto array = make(CgType::ArrayInfo { u8(), info.padding });
	if (!array) {
		return nullptr;
	}
	LLVM::TypeRef ref = nullptr;
	if (auto find = m_llvm.GetTypeByName2(m_context, name.data())) {
		ref = find;
	} else if (auto type = m_llvm.StructCreateNamed(m_context, name.data())) {
		LLVM::TypeRef types[1] = {
			array->ref()
		};
		m_llvm.StructSetBody(type, types, countof(types), true);
		ref = type;
	} else {
		return nullptr;
	}
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(array)) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::PADDING,
		info.padding,
		1,
		0,
		move(types),
		ref
	};
}

CgType* CgTypeCache::make(CgType::FnInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.resize(2)) {
		return nullptr;
	}
	types[0] = info.args;
	types[1] = info.rets;

	ScratchAllocator scratch{m_cache.allocator()};
	Array<LLVM::TypeRef> args{scratch};
	Bool has_va = false;
	for (Ulen l = info.args->length(), i = 0; i < l; i++) {
		auto arg = info.args->at(i);
		if (arg->is_padding()) {
			continue;
		}
		if (arg->is_va()) {
			has_va = true;
			break;
		}
		if (!args.push_back(arg->ref())) {
			return nullptr;
		}
	}

	auto ref = m_llvm.FunctionType(info.rets->length() == 1
	                                 ? info.rets->at(0)->ref()
	                                 : info.rets->ref(),
	                               args.data(),
	                               args.length(),
	                               has_va);
	if (!ref) {
		return nullptr;
	}
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::FN,
		0,
		0,
		0,
		move(types),
		ref
	};
}

CgType* CgTypeCache::make(CgType::VaInfo) noexcept {
	auto dst = m_cache.allocate();
	if (!dst) {
		return nullptr;
	}
	return new(dst, Nat{}) CgType {
		CgType::Kind::VA,
		0,
		0,
		0,
		None{},
		nullptr
	};
}

} // namespace Biron