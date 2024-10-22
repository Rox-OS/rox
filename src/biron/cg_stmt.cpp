#include <biron/ast_stmt.h>
#include <biron/ast_expr.h>
#include <biron/ast_attr.h>
#include <biron/ast_const.h>
#include <biron/ast_unit.h>
#include <biron/ast_type.h>

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

Bool AstBlockStmt::codegen(Cg& cg) const noexcept {
	// We generate a scope for each new block we add
	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}
	for (auto stmt : m_stmts) {
		if (!stmt->codegen(cg)) {
			return false;
		}
	}

	// Generate the defers for this scope if any
	if (!cg.scopes.last().emit_defers(cg)) {
		return false;
	}

	return cg.scopes.pop_back();
}

Bool AstReturnStmt::codegen(Cg& cg) const noexcept {
	CgType* return_type = nullptr;
	for (auto fn : cg.fns) {
		if (fn.node() == cg.fn) {
			return_type = fn.addr().type()->deref()->at(3);
			break;
		}
	}

	if (!return_type) {
		return cg.error(range(), "Could not infer return type");
	}

	Maybe<CgValue> value;
	if (m_expr) {
		value = m_expr->gen_value(cg, return_type);
		if (!value) {
			return false;
		}
	}

	// Generate all the defer statements in reverse order here before the return
	// but after we have generated the return value.
	for (Ulen l = cg.scopes.length(), i = l - 1; i < l; i--) {
		if (!cg.scopes[i].emit_defers(cg)) {
			return false;
		}
	}

	if (value) {
		// When the destination type is a union and our value type is not we need
		// to construct a union on the stack and assign to it our value. This stack
		// copy will then be returned from the function.
		if (return_type->is_union()) {
			auto result = cg.emit_alloca(return_type);
			result.store(cg, *value);
			cg.llvm.BuildRet(cg.builder, result.load(cg).ref());
		} else if (return_type->is_tuple() && return_type->length() == 1) {
			// When a function returns a single-element tuple we actually compile it
			// to a function which returns that element directly. So here we need to
			// return the element and not the tuple.
			cg.llvm.BuildRet(cg.builder, value->at(cg, 0)->ref());
		} else {
			cg.llvm.BuildRet(cg.builder, value->ref());
		}
	} else {
		cg.llvm.BuildRetVoid(cg.builder);
	}

	return true;
}

Bool AstDeferStmt::codegen(Cg& cg) const noexcept {
	return cg.scopes.last().defers.push_back(m_stmt);
}

Bool AstBreakStmt::codegen(Cg& cg) const noexcept {
	if (auto loop = cg.loop()) {
		cg.llvm.BuildBr(cg.builder, loop->exit);
		return true;
	}
	return cg.error(range(), "Cannot 'break' from outside a loop");
}

Bool AstContinueStmt::codegen(Cg& cg) const noexcept {
	if (auto loop = cg.loop()) {
		cg.llvm.BuildBr(cg.builder, loop->post);
		return true;
	}
	return cg.error(range(), "Cannot 'continue' from outside a loop");
}

Bool AstIfStmt::codegen(Cg& cg) const noexcept {

	if (m_init) {
		// When there is a 'let' statement we introduce another scope.
		//
		// if let ident = ...; expr {
		//   on_join();
		// } else {
		//   on_else();
		// }
		// on_exit();
		//
		// Compiles to
		//
		// {
		//   let ident = ...;
		//   if expr {
		//     on_join();
		//   } else {
		//     on_else();
		//   }
		// }
		// on_exit();

		if (!cg.scopes.emplace_back(cg.allocator)) {
			return false;
		}

		if (!m_init->codegen(cg)) {
			return false;
		}
	}

	auto cond = m_expr->gen_value(cg, cg.types.b32());
	if (!cond) {
		return false;
	}

	auto this_bb = cg.llvm.GetInsertBlock(cg.builder);
	auto this_fn = cg.llvm.GetBasicBlockParent(this_bb);
	auto then_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "then");
	auto join_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "join");
	auto else_bb = join_bb;

	cg.llvm.AppendExistingBasicBlock(this_fn, then_bb);

	if (m_elif) {
		else_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "else");
		cg.llvm.BuildCondBr(cg.builder, cond->ref(), then_bb, else_bb);
	} else {
		cg.llvm.BuildCondBr(cg.builder, cond->ref(), then_bb, join_bb);
	}

	cg.llvm.PositionBuilderAtEnd(cg.builder, then_bb);
	if (!m_then->codegen(cg)) {
		return false;
	}

	then_bb = cg.llvm.GetInsertBlock(cg.builder);
	if (!cg.llvm.GetBasicBlockTerminator(then_bb)) {
		cg.llvm.BuildBr(cg.builder, join_bb);
	}

	// We clear the current scope tests before the 'else'
	cg.scopes.last().tests.clear();

	if (m_elif) {
		cg.llvm.AppendExistingBasicBlock(this_fn, else_bb);
		cg.llvm.PositionBuilderAtEnd(cg.builder, else_bb);
		if (!m_elif->codegen(cg)) {
			return false;
		}
		else_bb = cg.llvm.GetInsertBlock(cg.builder);
		if (!cg.llvm.GetBasicBlockTerminator(else_bb)) {
			cg.llvm.BuildBr(cg.builder, join_bb);
		}
	}

	cg.llvm.AppendExistingBasicBlock(this_fn, join_bb);
	cg.llvm.PositionBuilderAtEnd(cg.builder, join_bb);

	if (m_init) {
		cg.scopes.pop_back();
	}

	return true;
}

Bool AstLLetStmt::codegen(Cg& cg) const noexcept {
	// When the initializer is an AstAggExpr or AstTupleExpr we can generate the
	// storage in-place and assign that as our CgVar skipping a copy.
	Maybe<CgAddr> addr;
	if (m_init->is_expr<AstAggExpr>() || m_init->is_expr<AstTupleExpr>()) {
		addr = m_init->gen_addr(cg, nullptr);
		if (!addr) {
			return false;
		}
	} else {
		// See if we can generate llvm.memcpy when the rhs is an array or tuple
		auto type = m_init->gen_type(cg, nullptr);
		if (!type) {
			return false;
		}
		addr = cg.emit_alloca(type);
		if (type->is_tuple() || type->is_array()) {
			auto src = m_init->gen_addr(cg, nullptr);
			if (src) {
				cg.llvm.BuildMemCpy(cg.builder,
				                    addr->ref(),
				                    type->align(),
				                    src->ref(),
				                    type->align(),
				                    cg.llvm.ConstInt(cg.types.u64()->ref(),
				                                     type->size(),
				                                     false));
			} else {
				// Some cases we cannot generate an address. Those cases we use gen_value
				// and CgAddr::store. CgAddr::store will extractvalue and perform a series
				// of stores recursively.
				goto L_value;
			}
		} else {
L_value:
			// Otherwise generate a value and store it.
			auto value = m_init->gen_value(cg, type);
			if (!value || !addr->store(cg, *value)) {
				return false;
			}
		}
	}
	for (const auto& attr : m_attrs) {
		if (attr->name() != "align") {
			return cg.error(range(), "Unknown attribute '%S' for 'let'", attr->name());
		}
		auto eval = attr->eval(cg);
		if (!eval || !eval->is_integral()) {
			return cg.error(eval->range(), "Expected integer constant expression in attribute");
		}
		cg.llvm.SetAlignment(addr->ref(), *eval->to<Uint64>());
		break;
	}
	if (!cg.scopes.last().vars.emplace_back(this, m_name, move(*addr))) {
		return false;
	}
	return true;
}

Bool AstGLetStmt::codegen(Cg& cg) const noexcept {
	auto eval = m_init->eval_value(cg);
	if (!eval) {
		return cg.error(m_init->range(), "Expected constant expression");
	}

	auto type = m_init->gen_type(cg, nullptr);
	if (!type) {
		return false;
	}

	auto src = eval->codegen(cg, type);
	if (!src) {
		return false;
	}

	auto dst = cg.llvm.AddGlobal(cg.module, type->ref(), cg.nameof(m_name));

	auto addr = CgAddr { src->type()->addrof(cg), dst };
	if (!cg.globals.emplace_back(CgVar { this, m_name, move(addr) }, move(*eval))) {
		return cg.oom();
	}

	cg.llvm.SetInitializer(dst, src->ref());
	cg.llvm.SetLinkage(dst, LLVM::Linkage::Private);
	for (auto attr : m_attrs) {
		if (attr->name() == "section") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_string()) {
				return cg.error(eval->range(), "Expected string constant expression in attribute");
			}
			auto value = eval->to<StringView>();
			auto name = value->terminated(*cg.scratch);
			if (name) {
				cg.llvm.SetSection(dst, name);
				continue;
			} else {
				return cg.oom();
			}
		} else if (attr->name() == "align") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_integral()) {
				return cg.error(eval->range(), "Expected integer constant expression in attribute");
			}
			cg.llvm.SetAlignment(dst, *eval->to<Uint64>());
			continue;
		} else if (attr->name() == "used") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				return cg.error(eval->range(), "Expected boolean constant expression in attribute");
			}
			// TODO(dweiler): Figure out how to mark 'dst' as used.
			continue;
		} else if (attr->name() == "export") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				return cg.error(eval->range(), "Expected boolean constant expression in attribute");
			}
			if (eval->as_bool()) {
				cg.llvm.SetLinkage(dst, LLVM::Linkage::External);
			}
			continue;
		}
		return cg.error(attr->range(), "Unknown attribute");
	}

	return true;
}

Bool AstUsingStmt::codegen(Cg& cg) const noexcept {
	CgType* type = nullptr;
	for (const auto& effect : cg.effects) {
		if (effect.name() == m_name) {
			type = effect.type();
			break;
		}
	}
	if (!type) {
		return cg.error(range(), "Undeclared effect '%S'", m_name);
	}

	auto addr = cg.emit_alloca(type);
	auto value = m_init->gen_value(cg, type);
	if (!addr.store(cg, *value)) {
		return false;
	}
	if (!cg.scopes.last().usings.emplace_back(this, m_name, move(addr))) {
		return false;
	}
	return true;
}

Bool AstExprStmt::codegen(Cg& cg) const noexcept {
	if (auto expr = m_expr->to_expr<const AstTupleExpr>()) {
		if (expr->length() == 0) {
			// The unit tuple as an expression is a no-op.
			return true;
		}
	}
	if (m_expr->gen_value(cg, nullptr)) {
		return true;
	}
	return false;
}

Bool AstAssignStmt::codegen(Cg& cg) const noexcept {
	auto dst = m_dst->gen_addr(cg, nullptr);
	if (!dst) {
		return false;
	}

	auto dst_type = dst->type()->deref();

	auto src = m_src->gen_value(cg, dst_type);
	if (!src) {
		return false;
	}

	auto src_type = src->type();
	if (dst_type->is_atomic()) {
		return cg.error(range(), "Cannot assign to atomic type");
	}

	// When the destination is a union type look for a compatible inner type.
	if (dst_type->is_union()) {
		if (auto find = dst_type->contains(src_type)) {
			dst_type = find;
		}
	}

	if (*dst_type != *src_type) {
		auto dst_type_string = dst_type->to_string(*cg.scratch);
		auto src_type_string = src_type->to_string(*cg.scratch);
		return cg.error(range(),
		                "Cannot assign an lvalue of type '%S' to an rvalue of type '%S'",
		                src_type_string,
		                dst_type_string);
	}

	switch (m_op) {
	case StoreOp::WR:
		return dst->store(cg, *src);
	case StoreOp::ADD:
		if (auto value = cg.emit_add(dst->load(cg), *src, range())) {
			return dst->store(cg, *value);
		}
		break;
	case StoreOp::SUB:
		if (auto value = cg.emit_sub(dst->load(cg), *src, range())) {
			return dst->store(cg, *value);
		}
		break;
	case StoreOp::MUL:
		if (auto value = cg.emit_mul(dst->load(cg), *src, range())) {
			return dst->store(cg, *value);
		}
		break;
	case StoreOp::DIV:
		if (auto value = cg.emit_div(dst->load(cg), *src, range())) {
			return dst->store(cg, *value);
		}
	}
	
	return false;
}

Bool AstForStmt::codegen(Cg& cg) const noexcept {
	// We always generate a scope outside for this statement since it may have an
	// optional init-stmt which should be scoped to the for block only
	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}
	// <init-stmt>?
	// loop:
	//	IF cond {
	//		br cond, join, else
	//	} ELSE {
	//		br join
	//	}
	// join:
	//	<body-stmt>
	//	br post
	// post:
	//	<post-stmt>?
	//	br loop
	// else:
	//	<else-stmt>?
	//	br exit
	// exit:
	//
	if (m_init && !m_init->codegen(cg)) {
		return false;
	}

	auto this_bb = cg.llvm.GetInsertBlock(cg.builder);
	auto this_fn = cg.llvm.GetBasicBlockParent(this_bb);

	auto loop_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "loop");
	auto join_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "join");
	auto post_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "post");
	auto else_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "else");
	auto exit_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "exit");

	cg.scopes.last().loop.emplace(post_bb, exit_bb);

	cg.llvm.BuildBr(cg.builder, loop_bb);

	cg.llvm.AppendExistingBasicBlock(this_fn, loop_bb);
	cg.llvm.PositionBuilderAtEnd(cg.builder, loop_bb);
	if (m_expr) {
		auto cond = m_expr->gen_value(cg, cg.types.b32());
		if (!cond) {
			return false;
		}
		cg.llvm.BuildCondBr(cg.builder, cond->ref(), join_bb, else_bb);
	} else {
		cg.llvm.BuildBr(cg.builder, join_bb);
	}
	cg.llvm.PositionBuilderAtEnd(cg.builder, join_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, join_bb);
	if (!m_body->codegen(cg)) {
		return false;
	}

	cg.llvm.BuildBr(cg.builder, post_bb);

	cg.llvm.PositionBuilderAtEnd(cg.builder, post_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, post_bb);
	if (m_post && !m_post->codegen(cg)) {
		return false;
	}
	cg.llvm.BuildBr(cg.builder, loop_bb);

	cg.llvm.PositionBuilderAtEnd(cg.builder, else_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, else_bb);
	if (m_else && !m_else->codegen(cg)) {
		return false;
	}
	cg.llvm.BuildBr(cg.builder, exit_bb);

	cg.llvm.PositionBuilderAtEnd(cg.builder, exit_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, exit_bb);

	cg.scopes.pop_back();

	return true;
}

} // namespace Biron