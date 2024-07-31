#include <biron/util/unreachable.inl>

#include <biron/codegen.h>
#include <biron/ast.h>

#include <stdio.h>

namespace Biron {

Bool Unit::dump(StringBuilder& builder) const noexcept {
	for (Ulen l = fns.length(), i = 0; i < l; i++) {
		fns[i]->dump(builder, 0);
	}
	return builder.valid();
}

static LLVM::TargetRef target_from_triple(LLVM& llvm, const char* triple) noexcept {
	LLVM::TargetRef target;
	char* error = nullptr;
	if (llvm.GetTargetFromTriple(triple, &target, &error) != 0) {
		fprintf(stderr, "Could not find target: %s\n", error);
		llvm.DisposeMessage(error);
		return nullptr;
	}
	return target;
}

Codegen::Codegen(LLVM& llvm, Allocator& allocator, const char* triple) noexcept 
	: llvm{llvm}
	, context{llvm.ContextCreate()}
	, builder{llvm.CreateBuilderInContext(context)}
	, module{llvm.ModuleCreateWithNameInContext("Biron", context)}
	, t_i1{llvm.Int1TypeInContext(context), Type::SIGNED}
	, t_s8{llvm.Int8TypeInContext(context), Type::SIGNED}
	, t_s16{llvm.Int16TypeInContext(context), Type::SIGNED}
	, t_s32{llvm.Int32TypeInContext(context), Type::SIGNED}
	, t_s64{llvm.Int64TypeInContext(context), Type::SIGNED}
	, t_u8{t_s8.type, Type::UNSIGNED}
	, t_u16{t_s16.type, Type::UNSIGNED}
	, t_u32{t_s32.type, Type::UNSIGNED}
	, t_u64{t_s64.type, Type::UNSIGNED}
	, t_ptr{llvm.PointerTypeInContext(context, 0), 0}
	, t_unit{llvm.VoidTypeInContext(context), 0}
	, t_slice{nullptr, 0}
	, machine{llvm.CreateTargetMachine(target_from_triple(llvm, triple),
	                                   triple,
	                                   "generic",
	                                   "",
	                                   LLVM::CodeGenOptLevel::Aggressive,
	                                   LLVM::RelocMode::PIC,
	                                   LLVM::CodeModel::Default)}
	, unit{nullptr}
	, vars{allocator}
	, fns{allocator}
	, types{allocator}
	, allocator{allocator}
{
	LLVM::TypeRef string_struct[] = { t_ptr.type, t_u64.type };
	t_slice.type = llvm.StructTypeInContext(context, string_struct, 2, false);
}

Codegen::~Codegen() {
	for (Ulen l = types.length(), i = 0; i < l; i++) {
		types[i]->~Type();
		allocator.deallocate(types[i], 0);
	}
	llvm.DisposeBuilder(builder);
	llvm.DisposeTargetMachine(machine);
	llvm.DisposeModule(module);
	llvm.ContextDispose(context);
}

Bool Codegen::optimize() noexcept {
	char* error = nullptr;
	if (llvm.VerifyModule(module,
	                      LLVM::VerifierFailureAction::PrintMessage,
	                      &error) != 0)
	{
		fprintf(stderr, "Could not verify module: %s\n", error);
		llvm.DisposeMessage(error);
		return false;
	}
	llvm.DisposeMessage(error);
	error = nullptr;
	auto options = llvm.CreatePassBuilderOptions();
	if (llvm.RunPasses(module, "default<O3>", machine, options)) {
		return false;
	}
	llvm.DisposePassBuilderOptions(options);
	return true;
}

Bool Codegen::emit(StringView name) noexcept {
	char* error = nullptr;
	if (llvm.VerifyModule(module,
	                      LLVM::VerifierFailureAction::PrintMessage,
	                      &error) != 0)
	{
		fprintf(stderr, "Could not verify module: %s\n", error);
		llvm.DisposeMessage(error);
		return false;
	}
	llvm.DisposeMessage(error);
	error = nullptr;

	if (llvm.TargetMachineEmitToFile(machine,
	                                 module,
	                                 name.terminated(allocator),
	                                 LLVM::CodeGenFileType::Object,
	                                 &error) != 0)
	{
		fprintf(stderr, "Could not compile: %s\n", error);
		llvm.DisposeMessage(error);
		return false;
	}
	llvm.DisposeMessage(error);
	error = nullptr;

	return true;
}

void Codegen::dump() noexcept {
	llvm.DumpModule(module);
}

Bool Codegen::run(Unit& inunit) noexcept {
	unit = &inunit;
	auto& fns = unit->fns;
	for (Ulen l = fns.length(), i = 0; i < l; i++) {
		if (!fns[i]->codegen(*this)) {
			return false;
		}
	}
	return true;
}

Maybe<Type> AstTupleType::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto explode = exploded(gen);
	if (!explode) {
		return None{};
	}

	Array<LLVM::TypeRef> types{gen.allocator};
	for (Ulen l = explode->length(), i = 0; i < l; i++) {
		auto& type = (*explode)[i];
		if (!types.push_back(type.type.type)) {
			return None{};
		}
	}

	return Type {
		llvm.StructTypeInContext(gen.context,
		                         types.data(),
		                         types.length(),
		                         false),
		0
	};
}

Maybe<Array<AstTupleType::Explode>> AstTupleType::exploded(Codegen& gen) noexcept {
	Array<Explode> explode{gen.allocator};
	for (Ulen l = elems.length(), i = 0; i < l; i++) {
		auto& elem = elems[i];
		auto type = elem.type->codegen(gen);
		if (!type) {
			return None{};
		}
		if (!explode.emplace_back(elem.name, *type)) {
			return None{};
		}
	}
	return explode;
}

Maybe<Type> AstIdentType::codegen(Codegen& gen) noexcept {
	if (ident == "Sint8")   return gen.t_s8;
	if (ident == "Uint8")   return gen.t_u8;

	if (ident == "Sint16")  return gen.t_s16;
	if (ident == "Uint16")  return gen.t_u16;

	if (ident == "Sint32")  return gen.t_s32;
	if (ident == "Uint32")  return gen.t_u32;

	if (ident == "Sint64")  return gen.t_s64;
	if (ident == "Uint64")  return gen.t_u64;

	if (ident == "Unit")    return gen.t_unit;
	if (ident == "Address") return gen.t_ptr;
	if (ident == "String")  return gen.t_slice;

	return None{};
}

Maybe<Type> AstVarArgsType::codegen(Codegen& gen) noexcept {
	(void)gen;
	return None{};
}

Maybe<Type> AstPtrType::codegen(Codegen& gen) noexcept {
	auto base = type->codegen(gen);
	if (!base) {
		return None{};
	}
	auto copy = gen.new_type(*base);
	if (!copy) {
		return None{};
	}
	return Type { gen.t_ptr.type, 0, copy };
}

Maybe<Type> AstArrayType::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto base = type->codegen(gen);
	if (!base) {
		return None{};
	}
	// We need to compile-time evaluate the AstExpr
	auto elems = extent->eval();
	if (!elems) {
		return None{};
	}
	return Type { llvm.ArrayType2(base->type, elems->as_u32), 0 };
}

Maybe<Type> AstSliceType::codegen(Codegen& gen) noexcept {
	return gen.t_slice;
}

// STMT CODEGEN
Bool AstBlockStmt::codegen(Codegen& gen) noexcept {
	for (Ulen l = stmts.length(), i = 0; i < l; i++) {
		if (!stmts[i]->codegen(gen)) {
			return false;
		}
	}
	return true;
}

Bool AstReturnStmt::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	if (expr) {
		auto value = expr->codegen(gen);
		if (!value) {
			return false;
		}
		llvm.BuildRet(gen.builder, value->value);
	} else {
		llvm.BuildRetVoid(gen.builder);
	}

	return true;
}

Bool AstDeferStmt::codegen(Codegen& gen) noexcept {
	(void)gen;
	return false; // TODO
}

Bool AstIfStmt::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	if (init) {
		if (!init->codegen(gen)) {
			return false;
		}
	}

	auto cond_v = expr->codegen(gen);
	if (!cond_v) {
		return false;
	}

	// When expr->type is not a i1 we will generate a comparison ne 0
	if (cond_v->type.type != gen.t_i1.type) {
		auto zero = llvm.ConstInt(cond_v->type.type, 0, false);
		cond_v->value = llvm.BuildICmp(gen.builder, LLVM::IntPredicate::NE, zero, cond_v->value, "");
	}

	// When we have an else statement we generate:
	//	br ? label %then, label %else
	//	then:
	//		...
	//		br label %merge
	//	else:
	//		...
	//		br label %merge
	//	merge:
	//
	// Otherwise we generate:
	//
	//	br ? label %then, label %merge
	//	then:
	//		...
	//		br label %merge
	//	merge:
	auto this_bb = llvm.GetInsertBlock(gen.builder);
	auto fn = llvm.GetBasicBlockParent(this_bb);
	auto then_bb = llvm.CreateBasicBlockInContext(gen.context, "then");
	llvm.AppendExistingBasicBlock(fn, then_bb);

	auto merge_bb = llvm.CreateBasicBlockInContext(gen.context, "merge");
	auto else_bb = merge_bb;
	if (elif) {
		else_bb = llvm.CreateBasicBlockInContext(gen.context, "else");
		llvm.BuildCondBr(gen.builder, cond_v->value, then_bb, else_bb);
	} else {
		llvm.BuildCondBr(gen.builder, cond_v->value, then_bb, merge_bb);
	}

	llvm.PositionBuilderAtEnd(gen.builder, then_bb);
	if (!then->codegen(gen)) {
		return false;
	}
	then_bb = llvm.GetInsertBlock(gen.builder);
	if (!llvm.GetBasicBlockTerminator(then_bb)) {
		llvm.BuildBr(gen.builder, merge_bb);
	}

	if (elif) {
		llvm.AppendExistingBasicBlock(fn, else_bb);
		llvm.PositionBuilderAtEnd(gen.builder, else_bb);
		if (!elif->codegen(gen)) {
			return false;
		}
		else_bb = llvm.GetInsertBlock(gen.builder);
		if (!llvm.GetBasicBlockTerminator(else_bb)) {
			llvm.BuildBr(gen.builder, merge_bb);
		}
	}

	llvm.AppendExistingBasicBlock(fn, merge_bb);
	llvm.PositionBuilderAtEnd(gen.builder, merge_bb);

	return true;
}

Bool AstAssignStmt::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	Maybe<Value> dst_v;
	if (dst->is_expr<AstUnaryExpr>()) {
		auto unary = static_cast<AstUnaryExpr*>(dst);
		if (unary->op == AstUnaryExpr::Operator::DEREF) {
			dst_v = unary->operand->codegen(gen);
		}
	} else {
		dst_v = dst->address(gen);
	}
	if (!dst_v) {
		return false;
	}
	auto src_v = src->codegen(gen);
	if (!src_v) {
		return false;
	}
	llvm.BuildStore(gen.builder, src_v->value, dst_v->value);
	return true;
}

Bool AstAsmStmt::codegen(Codegen& gen) noexcept {
	(void)gen;
	return true;
}

Bool AstLetStmt::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto value = init->codegen(gen);
	if (!value) {
		return false;
	}
	auto store = llvm.BuildAlloca(gen.builder, value->type.type, "");
	if (!gen.vars.emplace_back(name, value->type, store)) {
		return false;
	}
	llvm.BuildStore(gen.builder, value->value, store);
	return true;
}

Bool AstForStmt::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	auto bb = llvm.GetInsertBlock(gen.builder);
	auto function = llvm.GetBasicBlockParent(bb);

	if (init) {
		if (!(init->codegen(gen))) {
			return false;
		}
	}

	// Standard for expr loop
	if (expr) {
		auto for_test_bb = llvm.CreateBasicBlockInContext(gen.context, "for_test");
		auto for_exit_bb = llvm.CreateBasicBlockInContext(gen.context, "for_exit");
		auto for_body_bb = llvm.CreateBasicBlockInContext(gen.context, "for_body");

		llvm.BuildBr(gen.builder, for_test_bb);

		llvm.AppendExistingBasicBlock(function, for_test_bb);
		llvm.PositionBuilderAtEnd(gen.builder, for_test_bb);
		auto cond = expr->codegen(gen);
		if (!cond) {
			return false;
		}
		for_test_bb = llvm.GetInsertBlock(gen.builder);
		llvm.PositionBuilderAtEnd(gen.builder, for_test_bb);
		llvm.BuildCondBr(gen.builder, cond->value, for_body_bb, for_exit_bb);

		llvm.AppendExistingBasicBlock(function, for_body_bb);
		llvm.PositionBuilderAtEnd(gen.builder, for_body_bb);
		auto b = body->codegen(gen);
		if (!b) {
			return false;
		}
		for_body_bb = llvm.GetInsertBlock(gen.builder);

		// We do not have continue statement support yet so we can just put the
		// post condition after the body for now. In the future we will need to have
		// a common for_post_bb block which contains the post and a br back to the
		// top so that continue can br to it too.
		if (post) {
			if (!(post->codegen(gen))) {
				return false;
			}
		}

		llvm.PositionBuilderAtEnd(gen.builder, for_body_bb);
		llvm.BuildBr(gen.builder, for_test_bb); // Back to test

		llvm.AppendExistingBasicBlock(function, for_exit_bb);
		llvm.PositionBuilderAtEnd(gen.builder, for_exit_bb);
	} else {
		auto for_body_bb = llvm.CreateBasicBlockInContext(gen.context, "for_body");
		llvm.BuildBr(gen.builder, for_body_bb);
		llvm.AppendExistingBasicBlock(function, for_body_bb);
		llvm.PositionBuilderAtEnd(gen.builder, for_body_bb);
		auto b = body->codegen(gen);
		if (!b) {
			return false;
		}
		llvm.PositionBuilderAtEnd(gen.builder, for_body_bb);
		llvm.BuildBr(gen.builder, for_body_bb);
	}

	return true;
}

Bool AstExprStmt::codegen(Codegen& gen) noexcept {
	if (!expr->codegen(gen)) {
		return false;
	}
	return true;
}

// EXPR
Maybe<Array<Value>> AstTupleExpr::explode(Codegen& gen) noexcept {
	Array<Value> values{gen.allocator};
	for (Ulen l = exprs.length(), i = 0; i < l; i++) {
		if (auto expr = exprs[i]->codegen(gen)) {
			if (!values.push_back(move(*expr))) {
				return None{};
			}
		} else {
			return None{};
		}
	}
	return values;
}
Maybe<Value> AstTupleExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	auto explosion = explode(gen);
	if (!explosion) {
		return None{};
	}

	Array<LLVM::TypeRef> ts{gen.allocator};
	Array<LLVM::ValueRef> vs{gen.allocator};
	if (!ts.resize(explosion->length()) || !vs.resize(explosion->length())) {
		return None{};
	}
	for (Ulen l = explosion->length(), i = 0; i < l; i++) {
		auto& exploded = (*explosion)[i];
		ts[i] = exploded.type.type;
		vs[i] = exploded.value;
	}

	// Create a structure matching the tuple.
	auto t = Type {
		llvm.StructTypeInContext(gen.context, ts.data(), ts.length(), false),
		0,
	};
	auto v = llvm.BuildAlloca(gen.builder, t.type, "tuple");
	for (Ulen l = explosion->length(), i = 0; i < l; i++) {
		auto& exploded = (*explosion)[i];
		// Generate stores into the structure of the values.
		LLVM::ValueRef indices[] = {
			llvm.ConstInt(gen.t_u32.type, 0, false),
			llvm.ConstInt(gen.t_u32.type, i, false),
		};
		auto ptr = llvm.BuildGEP2(gen.builder, t.type, v, indices, 2, "tuple_elem");
		llvm.BuildStore(gen.builder, exploded.value, ptr);
	}

	return Value { t, llvm.BuildLoad2(gen.builder, t.type, v, "") };
}

Maybe<Value> AstCallExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	auto callee_value = callee->codegen(gen);
	if (!callee_value) {
		return None{};
	}
	// We don't actually generate a tuple expression when making a function call,
	// we explode the tuple into the function call instead.
	if (args) {
		auto args_value = args->explode(gen);
		if (!args_value) {
			return None{};
		}

		Array<LLVM::ValueRef> values{gen.allocator};
		if (!values.resize(args_value->length())) {
			return None{};
		}

		// When calling "printf" extract the ptr from the String
		for (Ulen l = values.length(), i = 0; i < l; i++) {
			auto& value = (*args_value)[i];
			if (c && value.type.type == gen.t_slice.type) {
				LLVM::ValueRef indices[] = {
					llvm.ConstInt(gen.t_u32.type, 0, false),
					llvm.ConstInt(gen.t_u32.type, 0, false),
				};
				// TODO(dweiler): Some form of "explode" which gives the addresses
				auto tmp = llvm.BuildAlloca(gen.builder, value.type.type, "");
				llvm.BuildStore(gen.builder, value.value, tmp);
				auto ptr = llvm.BuildGEP2(gen.builder, value.type.type, tmp, indices, 2, "string_ptr");
				values[i] = llvm.BuildLoad2(gen.builder, gen.t_ptr.type, ptr, "");
			} else {
				values[i] = value.value;
			}
		}

		auto value = llvm.BuildCall2(gen.builder,
		                             callee_value->type.type,
		                             callee_value->value,
		                             values.data(),
		                             values.length(),
		                             "");

		return Value { callee_value->type, value };
	}

	auto value = llvm.BuildCall2(gen.builder,
	                             callee_value->type.type,
	                             callee_value->value,
	                             nullptr,
	                             0,
	                             "");

	return Value { callee_value->type, value };
}

Maybe<Value> AstVarExpr::address(Codegen& gen) noexcept {
	for (Ulen l = gen.vars.length(), i = 0; i < l; i++) {
		const auto& var = gen.vars[i];
		if (var.name == name) {
			return var.value;
		}
	}

	printf("%zu:%zu: Could not find symbol: '%.*s'\n",
		range.beg(),
		range.end(),
		(int)name.length(), name.data());
	
	return None{};
}

Maybe<Value> AstVarExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	// Special behavior for 'printf' for now.
	if (name == "printf") {
		auto type = Type { llvm.FunctionType(gen.t_s32.type, &gen.t_ptr.type, 1, true), 0 };
		auto value = llvm.GetNamedFunction(gen.module, "printf");
		if (!value) {
			value = llvm.AddFunction(gen.module, "printf", type.type);
		}
		return Value { type, value };
	}

	// O(n) lookup for the variable by name
	for (Ulen l = gen.vars.length(), i = 0; i < l; i++) {
		const auto& var = gen.vars[i];
		if (var.name == name) {
			auto value = llvm.BuildLoad2(gen.builder, var.value.type.type, var.value.value, "");
			return Value { var.value.type, value };
		}
	}

	// Lookup function by name
	for (Ulen l = gen.fns.length(), i = 0; i < l; i++) {
		auto& fn = gen.fns[i];
		if (fn.name == name) {
			// We always return functions by address.
			return Value { fn.value.type, fn.value.value };
		}
	}

	printf("%zu:%zu: Could not find symbol: '%.*s'\n",
		range.beg(),
		range.end(),
		(int)name.length(), name.data());

	return None{};
}

Maybe<Value> AstIntExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto v = llvm.ConstInt(gen.t_s32.type, value, false);
	return Value { gen.t_s32, v };
}

Maybe<Const> AstIntExpr::eval() noexcept {
	return Const { value };
}

Maybe<Value> AstStrExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	char *terminated = literal.terminated(gen.allocator);
	if (!terminated) {
		return None{};
	}
	Ulen l = 0;
	for (Ulen i = 0; i < literal.length(); i++) {
		if (literal[i] == '\\') {
			i++; // Skip '\'
			switch (literal[i]) {
			/****/ case 'n': terminated[l++] = '\n'; // We only support \n and "
			break; case '"': terminated[l++] = '\"';
			}
		} else {
			terminated[l++] = literal[i];
		}
	}
	terminated[l] = '\0';
	LLVM::ValueRef values[] = {
		llvm.BuildGlobalString(gen.builder, terminated, ""),
		llvm.ConstInt(gen.t_u64.type, literal.length(), false),
	};
	auto type = gen.t_slice;
	auto value = llvm.ConstStructInContext(gen.context, values, 2, false);
	return Value { type, value };
}

Maybe<Value> AstBinExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	// When working with binary expressions we want to explode single-element
	// tuple expressions into their inner expressions. This is because the use of
	// parentheses in a binary expression is used to control the evaluation order
	// and we treat any parenthesized expression as a tuple.
	if (lhs->is_expr<AstTupleExpr>()) {
		lhs = static_cast<AstTupleExpr*>(lhs)->exprs[0];
	}
	if (rhs->is_expr<AstTupleExpr>()) {
		rhs = static_cast<AstTupleExpr*>(rhs)->exprs[0];
	}

	// Special case for 'as' since we do not want to emit the RHS as an expression.
	if (op == Operator::AS) {
		if (!rhs->is_expr<AstTypeExpr>()) {
			return None{};
		}
		auto lhs_v = lhs->codegen(gen);
		if (!lhs_v) {
			return None{};
		}
		auto type = static_cast<AstTypeExpr&>(*rhs);
		auto cast_t = type.type->codegen(gen);
		if (!cast_t) {
			return None{};
		}
		// Determine the cast opcode to use from LLVM.
		auto op = llvm.GetCastOpcode(lhs_v->value, false, cast_t->type, false);
		auto cast_v = llvm.BuildCast(gen.builder, op, lhs_v->value, cast_t->type, "");
		return Value { *cast_t, cast_v };
	}

	auto lhs_v = lhs->codegen(gen);
	if (!lhs_v) {
		return None{};
	}
	auto rhs_v = rhs->codegen(gen);
	if (!rhs_v) {
		return None{};
	}
	switch (op) {
	case Operator::ADD:
		return Value { lhs_v->type, llvm.BuildAdd(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::SUB:
		return Value { lhs_v->type, llvm.BuildSub(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::MUL:
		return Value { lhs_v->type, llvm.BuildMul(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::EQEQ:
		return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::EQ, lhs_v->value, rhs_v->value, "") };
	case Operator::NEQ:
		return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::NE, lhs_v->value, rhs_v->value, "") };
	case Operator::GT:
		if (lhs_v->type.flags & Type::UNSIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::UGT, lhs_v->value, rhs_v->value, "") };
		} else if (lhs_v->type.flags & Type::SIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::SGT, lhs_v->value, rhs_v->value, "") };
		}
		break;
	case Operator::GTE:
		if (lhs_v->type.flags & Type::UNSIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::UGE, lhs_v->value, rhs_v->value, "") };
		} else if (lhs_v->type.flags & Type::SIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::SGE, lhs_v->value, rhs_v->value, "") };
		}
		break;
	case Operator::LT:
		if (lhs_v->type.flags & Type::UNSIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::ULT, lhs_v->value, rhs_v->value, "") };
		} else if (lhs_v->type.flags & Type::SIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::SLT, lhs_v->value, rhs_v->value, "") };
		}
		break;
	case Operator::LTE:
		if (lhs_v->type.flags & Type::UNSIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::ULE, lhs_v->value, rhs_v->value, "") };
		} else if (lhs_v->type.flags & Type::SIGNED) {
			return Value { { gen.t_i1 }, llvm.BuildICmp(gen.builder, LLVM::IntPredicate::SLE, lhs_v->value, rhs_v->value, "") };
		}
		break;
	case Operator::LOR:  break; // Special behavior needed
	case Operator::LAND: break; // Special behavior needed
	case Operator::BOR:
		return Value { lhs_v->type, llvm.BuildOr(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::BAND:
		return Value { lhs_v->type, llvm.BuildAnd(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::LSHIFT:
		return Value { lhs_v->type, llvm.BuildShl(gen.builder, lhs_v->value, rhs_v->value, "") };
	case Operator::RSHIFT:
		// unsigned = logical shift
		// signed   = arithmetic shift
		if (lhs_v->type.flags & Type::UNSIGNED) {
			return Value { lhs_v->type, llvm.BuildLShr(gen.builder, lhs_v->value, rhs_v->value, "") };
		} else if (lhs_v->type.flags & Type::SIGNED) {
			return Value { lhs_v->type, llvm.BuildAShr(gen.builder, lhs_v->value, rhs_v->value, "") };
		}
		break;
	default:
		break;
	}
	fprintf(stderr, "Unimplemented binary operator: %d for given types\n", (int)op);
	return None{};
}

Maybe<Value> AstUnaryExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	if (op == Operator::ADDROF) {
		auto value = operand->address(gen);
		if (!value) {
			return None{};
		}
		auto base = gen.new_type(value->type);
		if (!base) {
			return None{};
		}
		return Value { Type { gen.t_ptr.type, 0, base }, value->value };
	}
	auto value = operand->codegen(gen);
	if (!value) {
		return None{};
	}
	switch (op) {
	case Operator::NEG:
		return Value { value->type, llvm.BuildNeg(gen.builder, value->value, "") };
	case Operator::NOT:
		return Value { value->type, llvm.BuildNot(gen.builder, value->value, "") };
	case Operator::DEREF: {
		// When dereferencing we want the unqualified type.
		auto type = value->type.unqual();
		return Value { type, llvm.BuildLoad2(gen.builder, type.type, value->value, "") };
	}
	case Operator::ADDROF:
		// Handled above
		break;
	}
	return None{};
}

Maybe<Value> AstIndexExpr::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto lhs = operand->codegen(gen);
	if (!lhs) {
		return None{};
	}

	// We build GEP to index
	auto rhs = index->codegen(gen);
	if (!rhs) {
		return None{};
	}

	auto gep = llvm.BuildGEP2(gen.builder, lhs->type.type, lhs->value, &rhs->value, 1, "");
	auto value = llvm.BuildLoad2(gen.builder, lhs->type.type, gep, "");
	return Value { lhs->type.unqual(), value };
}

Maybe<Value> AstIndexExpr::address(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;
	auto lhs = operand->codegen(gen);
	if (!lhs) {
		return None{};
	}

	// We build GEP to index
	auto rhs = index->codegen(gen);
	if (!rhs) {
		return None{};
	}

	return Value { lhs->type, llvm.BuildGEP2(gen.builder, lhs->type.type, lhs->value, &rhs->value, 1, "") };
}

Bool AstFn::codegen(Codegen& gen) noexcept {
	auto& llvm = gen.llvm;

	// When the function has a return type use it, otherwise use the Unit type.
	auto t_ret = rtype ? rtype->codegen(gen) : gen.t_unit;
	if (!t_ret) {
		return false;
	}

	// We parse a tuple but we explode the tuple elements into the function as
	// parameters.
	LLVM::TypeRef fn_t  = nullptr;
	Maybe<Array<AstTupleType::Explode>> arg_types;
	if (type) {
		arg_types = type->exploded(gen);
		if (!arg_types) {
			return false;
		}
		Array<LLVM::TypeRef> types{gen.allocator};
		for (Ulen l = arg_types->length(), i = 0; i < l; i++) {
			auto& arg_type = (*arg_types)[i];
			if (!types.push_back(arg_type.type.type)) {
				return false;
			}
		}
		fn_t = llvm.FunctionType(t_ret->type, types.data(), types.length(), false);
	} else {
		fn_t = llvm.FunctionType(t_ret->type, nullptr, 0, false);
	}

	auto name_c = name.terminated(gen.allocator);
	if (!name_c) {
		return false;
	}

	auto fn_v = llvm.AddFunction(gen.module, name_c, fn_t);
	auto bb = llvm.CreateBasicBlockInContext(gen.context, "entry");
	llvm.AppendExistingBasicBlock(fn_v, bb);
	llvm.PositionBuilderAtEnd(gen.builder, bb);

	// Make stack allocated copy of our arguments
	if (type) {
		gen.vars.clear();
		for (Ulen i = 0; i < type->elems.length(); i++) {
			const auto &t = (*arg_types)[i];
			auto src = llvm.GetParam(fn_v, i);
			auto dst = llvm.BuildAlloca(gen.builder, t.type.type, "");
			llvm.BuildStore(gen.builder, src, dst);
			// Only add ones with names to our symbol table.
			if (t.name && !gen.vars.emplace_back(*t.name, t.type, dst)) {
				return false;
			}
		}
	}

	if (!body->codegen(gen)) {
		return false;
	}

	bb = llvm.GetInsertBlock(gen.builder);

	// Ensure the function ends with a ret instruction
	if (!llvm.GetBasicBlockTerminator(bb)) {
		if (t_ret->type == gen.t_unit.type) {
			llvm.BuildRetVoid(gen.builder);
		} else {
			llvm.BuildRet(gen.builder, llvm.ConstInt(t_ret->type, 0, false));
		}
	}

	return gen.fns.emplace_back(name, Type { fn_t, 0 }, fn_v);
}

} // namespace Biron