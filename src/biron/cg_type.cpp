#include <biron/cg.h>
#include <biron/ast_type.h>
#include <biron/ast_expr.h>
#include <biron/ast_const.h>
#include <biron/ast_unit.h>

#include <biron/util/string.h>
#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgTypeCache> CgTypeCache::make(Allocator& allocator, LLVM& llvm, LLVM::ContextRef context, Ulen capacity) noexcept {
	Cache cache{allocator, sizeof(CgType), capacity};
	// We will construct a "bootstrapping" CgTypeCache which will be used to
	// construct some builtin types which are always expected to exist.
	CgTypeCache bootstrap{move(cache), llvm, context};
	CgType* (&builtin)[countof(bootstrap.m_builtin)] = bootstrap.m_builtin;
	builtin[0]  = bootstrap.make(CgType::IntInfo    { { 1, 1 }, false, None{} }); // U8
	builtin[1]  = bootstrap.make(CgType::IntInfo    { { 2, 2 }, false, None{} }); // U16
	builtin[2]  = bootstrap.make(CgType::IntInfo    { { 4, 4 }, false, None{} }); // U32
	builtin[3]  = bootstrap.make(CgType::IntInfo    { { 8, 8 }, false, None{} }); // U64
	builtin[4]  = bootstrap.make(CgType::IntInfo    { { 1, 1 }, true, None{} });  // S8
	builtin[5]  = bootstrap.make(CgType::IntInfo    { { 2, 2 }, true, None{} });  // S16
	builtin[6]  = bootstrap.make(CgType::IntInfo    { { 4, 4 }, true, None{} });  // S32
	builtin[7]  = bootstrap.make(CgType::IntInfo    { { 8, 8 }, true, None{} });  // S64
	builtin[8]  = bootstrap.make(CgType::BoolInfo   { { 1, 1 }, None{} });
	builtin[9]  = bootstrap.make(CgType::BoolInfo   { { 2, 2 }, None{} });
	builtin[10] = bootstrap.make(CgType::BoolInfo   { { 4, 4 }, None{} });
	builtin[11] = bootstrap.make(CgType::BoolInfo   { { 8, 8 }, None{} });
	builtin[12] = bootstrap.make(CgType::RealInfo   { { 4, 4 }, None{} }),
	builtin[13] = bootstrap.make(CgType::RealInfo   { { 8, 8 }, None{} }),
	builtin[14] = bootstrap.make(CgType::PtrInfo    { { 8, 8 }, nullptr, None{} });
	builtin[15] = bootstrap.make(CgType::StringInfo { { 16, 8 }, None{} });
	builtin[16] = bootstrap.make(CgType::TupleInfo  { { allocator }, None {}, StringView { ".Unit" } } );
	builtin[17] = bootstrap.make(CgType::VaInfo     { });
	for (Ulen i = 0; i < countof(builtin); i++) {
		if (!builtin[i]) {
			return None{};
		}
	}
	return move(bootstrap);
}

CgTypeCache::~CgTypeCache() noexcept {
	for (const auto& type : m_cache) {
		static_cast<CgType*>(type)->~CgType();
	}
}

void CgType::dump(StringBuilder& builder) const noexcept {
	if (m_name) {
		builder.append(*m_name);
		return;
	}

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
		builder.append("Real32");
		break;
	case Kind::F64:
		builder.append("Real64");
		break;
	case Kind::STRING:
		builder.append("String");
		break;
	case Kind::POINTER:
		builder.append('*');
		if (m_types && m_types->length()) {
			const auto type = at(0);
			const auto group = type->is_union() && !type->name();
			if (group) {
				builder.append('(');
			}
			type->dump(builder);
			if (group) {
				builder.append(')');
			}
		}
		break;
	case Kind::ATOMIC:
		builder.append('@');
		at(0)->dump(builder);
		break;
	case Kind::SLICE:
		builder.append("[]");
		at(0)->dump(builder);
		break;
	case Kind::ARRAY:
		builder.append('[');
		builder.append(Uint64(m_extent));
		builder.append(']');
		at(0)->dump(builder);
		break;
	case Kind::PADDING:
		builder.append(".Pad");
		builder.append(Uint64(m_layout.size));
		break;
	case Kind::TUPLE:
		{
			builder.append('(');
			// Do not print the .Pad fields since this is used for user-facing
			// printing of types.
			Bool f = true;
			for (Ulen l = length(), i = 0; i < l; i++) {
				auto elem = at(i);
				if (!elem->is_padding()) {
					if (!f) builder.append(", ");
					elem->dump(builder);
				}
				f = false;
			}
			builder.append(')');
		}
		break;
	case Kind::UNION:
		for (Ulen l = length(), i = 0; i < l; i++) {
			at(i)->dump(builder);
			if (i != l - 1) {
				builder.append(" | ");
			}
		}
		break;
	case Kind::ENUM:
		{
			builder.append('[');
			Bool f = true;
			for (Ulen l = (*m_fields).length(), i = 0; i < l; i++) {
				const auto& field = (*m_fields)[i];
				if (!f) builder.append(", ");
				builder.append('.');
				builder.append(*field.name);
				f = false;
			}
			builder.append(']');
		}
		break;
	case Kind::FN:
		{
			Bool f;
			builder.append("fn");
			if (at(0)->length()) {
				builder.append('(');
				// objs
				const auto& objs = at(0)->types();
				f = true;
				for (const auto& obj : objs) {
					if (!f) {
						builder.append(", ");
					}
					obj->dump(builder);
					f = false;
				}
				builder.append(')');
			}
			builder.append('(');
			const auto& args = at(1)->types();
			f = true;
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
			const auto& rets = at(3)->types();
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

StringView CgType::to_string(Allocator& allocator) const noexcept {
	StringBuilder builder{allocator};
	dump(builder);
	if (builder.valid()) {
		return builder.view();
	}
	return "Out of memory";
}

CgType* CgType::addrof(Cg& cg) noexcept {
	return cg.types.make(CgType::PtrInfo { { 8, 8 }, this, None{} });
}

Bool CgType::operator!=(const CgType& other) const noexcept {
	if (other.m_kind != m_kind) {
		return true;
	}
	if (other.m_layout != m_layout) {
		return true;
	}
	if (other.m_extent != m_extent) {
		return true;
	}
	if (other.m_types) { 
		if (!m_types) {
			// Other has types but we do not.
			return true;
		}
		const auto& lhs = *other.m_types;
		const auto& rhs = *m_types;
		if (lhs.length() != rhs.length()) {
			// The type lists are not the same length.
			return true;
		}
		for (Ulen l = lhs.length(), i = 0; i < l; i++) {
			if (*lhs[i] != *rhs[i]) {
				// The types are not the same.
				return true;
			}
		}
	}
	// We do not compare m_ref
	return false;
}

CgType* AstTupleType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	if (m_elems.empty()) {
		return cg.types.unit();
	}

	Array<CgType*> types{cg.allocator};
	if (!types.reserve(m_elems.length())) {
		return nullptr;
	}
	Array<ConstField> fields{cg.allocator};
	if (!fields.reserve(m_elems.length())) {
		return nullptr;
	}
	for (auto& elem : m_elems) {
		auto type = elem.type()->codegen(cg, None{});
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			return cg.oom();
		}
		if (!fields.emplace_back(elem.name(), None{})) {
			return cg.oom();
		}
	}
	return cg.types.make(CgType::TupleInfo { move(types), move(fields), move(name) });
}

CgType* AstArgsType::codegen(Cg& cg, Maybe<StringView>) const noexcept {
	if (m_elems.empty()) {
		return cg.types.unit();
	}

	Array<CgType*> types{cg.allocator};
	if (!types.reserve(m_elems.length())) {
		return nullptr;
	}
	Array<ConstField> fields{cg.allocator};
	if (!fields.reserve(m_elems.length())) {
		return nullptr;
	}
	for (auto& elem : m_elems) {
		auto type = elem.type()->codegen(cg, None{});
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			return cg.oom();
		}
		if (!fields.emplace_back(elem.name(), None{})) {
			return cg.oom();
		}
	}
	return cg.types.make(CgType::TupleInfo { move(types), move(fields), None{} });
}

CgType* AstGroupType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	return m_type->codegen(cg, name);
}

CgType* AstUnionType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	Array<CgType*> types{cg.allocator};
	if (!types.reserve(m_types.length())) {
		return nullptr;
	}
	for (const auto elem : m_types) {
		auto type = elem->codegen(cg, None{});
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			return cg.oom();
		}
	}
	return cg.types.make(CgType::UnionInfo { move(types), name });
}

CgType* AstIdentType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	/**/ if (m_ident == "Uint8")   return cg.types.u8();
	else if (m_ident == "Uint16")  return cg.types.u16();
	else if (m_ident == "Uint32")  return cg.types.u32();
	else if (m_ident == "Uint64")  return cg.types.u64();
	else if (m_ident == "Sint8")   return cg.types.s8();
	else if (m_ident == "Sint16")  return cg.types.s16();
	else if (m_ident == "Sint32")  return cg.types.s32();
	else if (m_ident == "Sint64")  return cg.types.s64();
	else if (m_ident == "Bool8")   return cg.types.b8();
	else if (m_ident == "Bool16")  return cg.types.b16();
	else if (m_ident == "Bool32")  return cg.types.b32();
	else if (m_ident == "Bool64")  return cg.types.b64();
	else if (m_ident == "Real32")  return cg.types.f32();
	else if (m_ident == "Real64")  return cg.types.f64();
	else if (m_ident == "String")  return cg.types.str();
	else if (m_ident == "Address") return cg.types.ptr();
	else if (m_ident == "Length")  return cg.types.u64();
	for (auto type : cg.typedefs) {
		if (type.name() == m_ident) {
			return type.type();
		}
	}

	for (auto effect : cg.effects) {
		if (effect.name() == m_ident) {
			return effect.type();
		}
	}

	// Check the unit for non-generated types and generate them here. This will
	// basically perform an implicit dependency sort of the types for us for free.
	if (const auto typedefs = cg.ast->cache<AstTypedef>()) {
		for (auto opaque : *typedefs) {
			const auto type = static_cast<const AstTypedef*>(opaque);
			if (type->name() == m_ident) {
				if (type->codegen(cg)) {
					return codegen(cg, name);
				}
			}
		}
	}

	if (const auto effects = cg.ast->cache<AstEffect>()) {
		for (auto opaque : *effects) {
			const auto effect = static_cast<const AstEffect*>(opaque);
			if (effect->name() == m_ident) {
				if (effect->codegen(cg)) {
					return codegen(cg, name);
				}
			}
		}
	}

	return cg.error(range(), "Undeclared entity '%S'", m_ident, range().length);
}

CgType* AstVarArgsType::codegen(Cg&, Maybe<StringView>) const noexcept {
	// There is nothing to codegen for a va.
	return nullptr;
}

CgType* AstPtrType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	auto base = m_type->codegen(cg, None{});
	if (!base) {
		return nullptr;
	}
	return cg.types.make(CgType::PtrInfo { { 8, 8 }, base, name });
}

CgType* AstArrayType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	auto base = m_base->codegen(cg, None{});
	if (!base) {
		return nullptr;
	}
	auto value = m_extent->eval_value(cg);
	if (!value || !value->is_integral()) {
		return cg.error(m_extent->range(), "Expected integer constant expression for array extent");
	}
	auto extent = value->to<Uint64>();
	if (!extent) {
		// Cannot cast integer constant expression to Uint64 extent
		return nullptr;
	}
	return cg.types.make(CgType::ArrayInfo { base, *extent, name });
}

CgType* AstSliceType::codegen(Cg& cg, Maybe<StringView>) const noexcept {
	auto base = m_type->codegen(cg, None{});
	if (!base) {
		return nullptr;
	}
	return cg.types.make(CgType::SliceInfo { base });
}

CgType* AstFnType::codegen(Cg& cg, Maybe<StringView>) const noexcept {
	auto objs = m_objs->codegen(cg, None{});
	if (!objs) {
		return nullptr;
	}

	auto args = m_args->codegen(cg, None{});
	if (!args) {
		return nullptr;
	}

	// Generate a tuple for the effects as well
	CgType::TupleInfo info { *cg.scratch, { *cg.scratch }, None {} };
	for (auto effect : m_effects) {
		auto type = effect->codegen(cg, None{});
		if (!type) {
			return nullptr;
		}
		if (!info.types.push_back(type)) {
			return nullptr;
		}
		if (!info.fields->emplace_back(effect->name(), None{})) {
			return nullptr;
		}
	}
	auto effects = info.types.empty()
		? cg.types.unit()
		: cg.types.make(move(info));
	if (!effects) {
		return nullptr;
	}

	auto ret = m_ret->codegen(cg, None{});
	if (!ret) {
		return nullptr;
	}

	auto fn = cg.types.make(CgType::FnInfo { objs, args, effects, ret });
	if (!fn) {
		return nullptr;
	}

	// All function types are implicit pointer types. Thus to get the actual type
	// of a function you must type->deref() first.
	return cg.types.make(CgType::PtrInfo { { 8, 8 }, fn, None{} });
}

CgType* AstAtomType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	auto base = m_base->codegen(cg, None{});
	if (!base) {
		return nullptr;
	}
	if (!base->is_integer() && !base->is_pointer()) {
		auto type_string = base->to_string(*cg.scratch);
		return cg.error(m_base->range(), "Cannot have an atomic of type '%S'", type_string);
	}
	return cg.types.make(CgType::AtomicInfo { base, name });
}

CgType* AstEnumType::codegen(Cg& cg, Maybe<StringView> name) const noexcept {
	if (m_enums.empty()) {
		return cg.error(range(), "Cannot have an empty enum type");
	}

	CgType* type = cg.types.u64();
	Sint128 offset = 0;
	Array<ConstField> fields{*cg.scratch};
	for (const auto& enumerator : m_enums) {
		// We need to adjust
		if (auto& init = enumerator.init) {
			auto infer = init->gen_type(cg, type);
			if (!infer) {
				return cg.error(init->range(), "Cannot infer type for enumerator");
			}
			if (!type) {
				type = infer;
			}
			auto value = init->eval_value(cg);
			if (!value) {
				return cg.error(init->range(), "Expected constant expression for enumerator");
			}
			offset = *value->to<Sint128>();
		}
		if (!fields.emplace_back(enumerator.name, AstConst { range(), Sint64(offset) })) {
			return cg.oom();
		}
		offset++;
	}

	return cg.types.make(CgType::EnumInfo { type, move(fields), name });
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
		break;
	default:
		return nullptr;
	}
	return m_cache.make<CgType>(
		kind,
		info,
		0_ulen,
		None{},
		None{},
		info.named,
		ref
	);
}

CgType* CgTypeCache::make(CgType::RealInfo info) noexcept {
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
	default:
		return nullptr;
	}
	return m_cache.make<CgType>(
		kind,
		info,
		0_ulen,
		None{},
		None{},
		info.named,
		ref
	);
}

CgType* CgTypeCache::make(CgType::PtrInfo info) noexcept {
	Maybe<Array<CgType*>> types{m_cache.allocator()};
	if (info.base && !types.emplace(m_cache.allocator()).push_back(info.base)) {
		return nullptr;
	}
	auto ref = m_llvm.PointerTypeInContext(m_context, 0);
	return m_cache.make<CgType>(
		CgType::Kind::POINTER,
		info,
		0_ulen,
		move(types),
		None{},
		info.named,
		ref
	);
}

CgType* CgTypeCache::make(CgType::BoolInfo info) noexcept {
	auto ref = m_llvm.Int1TypeInContext(m_context);
	CgType::Kind kind;
	switch (info.size) {
	case 8:
		kind = CgType::Kind::B64;
		break;
	case 4:
		kind = CgType::Kind::B32;
		break;
	case 2:
		kind = CgType::Kind::B16;
		break;
	case 1:
		kind = CgType::Kind::B8;
		break;
	default:
		return nullptr;
	}
	return m_cache.make<CgType>(
		kind,
		info,
		0_ulen,
		None{},
		None{},
		info.named,
		ref
	);
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
		m_llvm.StructSetBody(type, types, countof(types), true);
		ref = type;
	} else {
		return nullptr;
	}
	Array<CgType*> types{m_cache.allocator()};
	if (!types.resize(2)) {
		return nullptr;
	}
	types[0] = make(CgType::PtrInfo { { 8, 8 }, u8(), None{} });
	types[1] = u64();
	return m_cache.make<CgType>(
		CgType::Kind::STRING,
		CgType::Layout {
			sum(ptr()->size(), u64()->size()),
			max(ptr()->align(), u64()->align())
		},
		0_ulen,
		move(types),
		None{},
		None{},
		ref
	);
}

CgType* CgTypeCache::make(CgType::TupleInfo info) noexcept {
	Array<CgType*> padded{m_cache.allocator()};
	Array<ConstField> fields{m_cache.allocator()};
	if (!padded.reserve(info.types.length())) {
		return nullptr;
	}
	if (!fields.reserve(info.types.length())) {
		return nullptr;
	}
	Ulen offset = 0;
	Ulen alignment = 0;
	Ulen index = 0;
	for (auto type : info.types) {
		if (!type->is_va()) {
			const auto align_mask = type->align() - 1;
			const auto aligned_offset = (offset + align_mask) & ~align_mask;
			if (auto padding = aligned_offset - offset) {
				auto pad = ensure_padding(padding);
				if (!pad || !padded.push_back(pad)) {
					return nullptr;
				}
				if (info.fields && !fields.emplace_back()) {
					return nullptr;
				}
			}
			offset = sum(aligned_offset, type->size());
			alignment = max(alignment, type->align());
		}
		if (!padded.push_back(type)) {
			return nullptr;
		}
		if (info.fields) {
			const auto& field = (*info.fields)[index];
			if (!fields.emplace_back(field.name, field.init.copy())) {
				return nullptr;
			}
		}
		index++;
	}
	const auto align_mask = alignment - 1;
	const auto aligned_offset = (offset + align_mask) & ~align_mask;
	if (auto padding = aligned_offset - offset) {
		auto pad = ensure_padding(padding);
		if (!pad || !padded.push_back(pad)) {
			return nullptr;
		}
		if (!fields.emplace_back()) {
			return nullptr;
		}
		offset = aligned_offset;
	}
	LLVM::TypeRef ref = nullptr;
	Maybe<StringView> name = info.named;
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
		if (name) {
			auto terminated = name->terminated(scratch);
			if (!terminated) {
				return nullptr;
			}
			ref = m_llvm.StructCreateNamed(m_context, terminated);
			m_llvm.StructSetBody(ref, types.data(), types.length(), true);
		} else {
			ref = m_llvm.StructTypeInContext(m_context, types.data(), types.length(), true);
		}
	}
	return m_cache.make<CgType>(
		CgType::Kind::TUPLE,
		CgType::Layout { offset, alignment },
		0_ulen,
		move(padded),
		move(fields),
		move(name),
		ref
	);
}

CgType* CgTypeCache::make(CgType::UnionInfo info) noexcept {
	Ulen size = 0;
	Ulen align = 0;
	for (auto type : info.types) {
		size = max(size, type->size());
		align = max(size, type->align());
	}

	auto array = make(CgType::ArrayInfo { u8(), size, None{} });
	if (!array) {
		return nullptr;
	}

	Array<CgType*> padded{m_cache.allocator()};
	if (!padded.push_back(array)) {
		return nullptr;
	}

	if (!padded.push_back(u8())) {
		return nullptr;
	}

	// We always use a u8 type tag but we still need to work out how many bytes
	// of padding we need to add after the tag so that an array of the union type
	// will be correctly aligned.
	Ulen offset = size + 1;
	const auto align_mask = align - 1;
	const auto aligned_offset = (offset + align_mask) & ~align_mask;
	if (auto padding = aligned_offset - offset) {
		// Shove padding on the end of the structure.
		auto pad = ensure_padding(padding);
		if (!pad || !padded.push_back(pad)) {
			return nullptr;
		}
	}

	InlineAllocator<sizeof(LLVM::TypeRef[16])> scratch;
	Array<LLVM::TypeRef> types{scratch};
	if (!types.resize(padded.length())) {
		return nullptr;
	}
	for (Ulen l = padded.length(), i = 0; i < l; i++) {
		types[i] = padded[i]->ref();
	}

	auto ref = m_llvm.StructTypeInContext(m_context,
	                                      types.data(),
	                                      types.length(),
	                                      true);

	// We don't store 'padded' in the CgType nested types list. It's only used for
	// the representation during codegen. We still store the list of variants in
	// the nested type list so we can lookup types and so dump / to_string prints
	// the sum type correctly.
	auto copy = info.types.copy();
	if (!copy) {
		return nullptr;
	}

	return m_cache.make<CgType>(
		CgType::Kind::UNION,
		CgType::Layout { offset, align },
		0_ulen,
		move(copy),
		None{},
		info.named,
		ref
	);
}

CgType* CgTypeCache::make(CgType::ArrayInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(info.base)) {
		return nullptr;
	}
	auto ref = m_llvm.ArrayType2(info.base->ref(), info.extent);
	return m_cache.make<CgType>(
		CgType::Kind::ARRAY,
		CgType::Layout {
			info.base->size() * info.extent,
			info.base->align()
		},
		info.extent,
		move(types),
		None{},
		info.named,
		ref
	);
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
	return m_cache.make<CgType>(
		CgType::Kind::SLICE,
		CgType::Layout {
			sum(ptr()->size(), u64()->size()),
			max(ptr()->align(), u64()->align())
		},
		0_ulen,
		move(types),
		None{},
		None{},
		ref
	);
}

CgType* CgTypeCache::make(CgType::PaddingInfo info) noexcept {
	StringBuilder name{m_cache.allocator()};
	name.append(".Pad");
	name.append(Uint64(info.padding));
	name.append('\0');
	if (!name.valid()) {
		return nullptr;
	}
	auto array = make(CgType::ArrayInfo { u8(), info.padding, name.view() });
	if (!array) {
		return nullptr;
	}
	LLVM::TypeRef ref = nullptr;
	if (auto find = m_llvm.GetTypeByName2(m_context, name.data())) {
		ref = find;
	} else {
		auto type = m_llvm.StructCreateNamed(m_context, name.data());
		LLVM::TypeRef types[1] = {
			array->ref()
		};
		m_llvm.StructSetBody(type, types, countof(types), true);
		ref = type;
	}
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(array)) {
		return nullptr;
	}
	return m_cache.make<CgType>(
		CgType::Kind::PADDING,
		CgType::Layout { info.padding, 1_ulen },
		0_ulen,
		move(types),
		None{},
		None{},
		ref
	);
}

CgType* CgTypeCache::make(CgType::FnInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.resize(4)) {
		return nullptr;
	}
	types[0] = info.objs;
	types[1] = info.args;
	types[2] = info.effects;
	types[3] = info.ret;

	ScratchAllocator scratch{m_cache.allocator()};
	Array<LLVM::TypeRef> args{scratch};
	Bool has_va = false;

	// The first argument will be a pointer to the tuple.
	if (info.effects != unit()) {
		auto type = make(CgType::PtrInfo { { 8, 8 }, types[2], None{} });
		if (!args.push_back(type->ref())) {
			return nullptr;
		}
	}

	// Emit all objs arguments first
	for (Ulen l = info.objs->length(), i = 0; i < l; i++) {
		auto arg = info.objs->at(i);
		if (arg->is_padding()) {
			continue;
		}
		if (!args.push_back(arg->ref())) {
			return nullptr;
		}
	}

	// Then emit the non-objs after
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

	auto rets = info.ret;

	// When a function returns a single-element tuple we actually compile it to a
	// function which returns that element directly. So here we need to specify
	// the type of the element and not the tuple.
	if (rets->length() == 1) {
		rets = rets->at(0);
	}

	auto ref = m_llvm.FunctionType(rets->ref(),
	                               args.data(),
	                               args.length(),
	                               has_va);

	return m_cache.make<CgType>(
		CgType::Kind::FN,
		CgType::Layout { 8_ulen, 8_ulen },
		0_ulen,
		move(types),
		None{},
		None{},
		ref
	);
}

CgType* CgTypeCache::make(CgType::VaInfo) noexcept {
	return m_cache.make<CgType>(
		CgType::Kind::VA,
		CgType::Layout { 0_ulen, 0_ulen },
		0_ulen,
		None{},
		None{},
		None{},
		nullptr
	);
}

CgType* CgTypeCache::make(CgType::AtomicInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(info.base)) {
		return nullptr;
	}
	return m_cache.make<CgType>(
		CgType::Kind::ATOMIC,
		info.base->layout(),
		0_ulen,
		move(types),
		None{},
		info.named,
		info.base->ref()
	);
}

CgType* CgTypeCache::make(CgType::EnumInfo info) noexcept {
	Array<CgType*> types{m_cache.allocator()};
	if (!types.push_back(info.base)) {
		return nullptr;
	}
	// We need to make a copy of the ConstField
	Array<ConstField> fields{m_cache.allocator()};
	for (const auto& field : info.fields) {
		auto copy = field.copy();
		if (!copy || !fields.push_back(move(*copy))) {
			return nullptr;
		}
	}
	return m_cache.make<CgType>(
		CgType::Kind::ENUM,
		info.base->layout(),
		0_ulen,
		move(types),
		move(fields),
		info.named,
		info.base->ref()
	);
}

CgType* CgTypeCache::ensure_padding(Ulen padding) noexcept {
	if (auto find = m_padding_cache.at(padding); find && *find) {
		return *find;
	}
	if (!m_padding_cache.resize(padding + 1)) {
		return nullptr;
	}
	auto pad = make(CgType::PaddingInfo { padding });
	if (!pad) {
		return nullptr;
	}
	m_padding_cache[padding] = pad;
	return pad;
};

} // namespace Biron