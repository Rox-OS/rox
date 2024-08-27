#include <biron/ast_stmt.h>
#include <biron/ast_expr.h>
#include <biron/ast_attr.h>
#include <biron/ast_const.h>

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
	// Generate all the defer statements in reverse order here before the return.
	for (Ulen l = cg.scopes.length(), i = l - 1; i < l; i--) {
		if (!cg.scopes[i].emit_defers(cg)) {
			return false;
		}
	}

	if (!m_expr) {
		cg.llvm.BuildRetVoid(cg.builder);
		return true;
	}

	auto value = m_expr->gen_value(cg);
	if (!value) {
		return false;
	}

	cg.llvm.BuildRet(cg.builder, value->ref());

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
	cg.error(range(), "Cannot 'break' from outside a loop");
	return false;
}

Bool AstContinueStmt::codegen(Cg& cg) const noexcept {
	if (auto loop = cg.loop()) {
		cg.llvm.BuildBr(cg.builder, loop->post);
		return true;
	}
	cg.error(range(), "Cannot 'continue' from outside a loop");
	return false;
}

Bool AstIfStmt::codegen(Cg& cg) const noexcept {
	if (m_init && !m_init->codegen(cg)) {
		return false;
	}

	auto cond = m_expr->gen_value(cg);
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

	return true;
}

Bool AstLetStmt::codegen(Cg& cg) const noexcept {
	// When the initializer is an AstAggExpr or AstTupleExpr we can generate the
	// storage in-place and assign that as our CgVar skipping a copy.
	Maybe<CgAddr> addr;
	if (m_init->is_expr<AstAggExpr>() || m_init->is_expr<AstTupleExpr>()) {
		addr = m_init->gen_addr(cg);
		if (!addr) {
			return false;
		}
	} else {
		auto value = m_init->gen_value(cg);
		if (!value) {
			return false;
		}
		addr = cg.emit_alloca(value->type());
		if (!addr) {
			return false;
		}
		if (!addr->store(cg, *value)) {
			return false;
		}
	}
	if (m_attrs) {
		for (const auto& it : *m_attrs) {
			if (it->is_attr<AstAlignAttr>()) {
				auto attr = static_cast<AstAlignAttr*>(it);
				cg.llvm.SetAlignment(addr->ref(), attr->value());
			} else {
				cg.error(range(), "Unknown attribute for 'let'");
			}
		}
	}
	if (!cg.scopes.last().vars.emplace_back(m_name, move(*addr))) {
		return false;
	}
	return true;
}

Bool AstLetStmt::codegen_global(Cg& cg) const noexcept {
	auto eval = m_init->eval();
	if (!eval) {
		cg.error(m_init->range(), "Expected constant expression");
		return false;
	}

	auto src = eval->codegen(cg);
	if (!src) {
		return false;
	}

	auto dst = cg.llvm.AddGlobal(cg.module,
	                             src->type()->ref(),
	                             m_name.terminated(cg.allocator));

	cg.llvm.SetInitializer(dst, src->ref());

	cg.llvm.SetGlobalConstant(dst, true);
	if (m_attrs) {
		for (auto it : *m_attrs) {
			if (it->is_attr<AstAlignAttr>()) {
				auto attr = static_cast<const AstAlignAttr*>(it);
				cg.llvm.SetAlignment(dst, attr->value());
			} else if (it->is_attr<AstSectionAttr>()) {
				auto attr = static_cast<const AstSectionAttr*>(it);
				cg.llvm.SetSection(dst, attr->value().terminated(cg.allocator));
			}
		}
	}

	return true;
}

Bool AstExprStmt::codegen(Cg& cg) const noexcept {
	// TODO(dweiler): Optimization to omit the value
	auto value = m_expr->gen_value(cg);
	return value.is_some();
}

Bool AstAssignStmt::codegen(Cg& cg) const noexcept {
	auto dst = m_dst->gen_addr(cg);
	if (!dst) {
		return false;
	}
	auto src = m_src->gen_value(cg);
	if (!src) {
		return false;
	}
	if (*dst->type()->deref() != *src->type()) {
		StringBuilder b0{cg.allocator};
		StringBuilder b1{cg.allocator};
		dst->type()->deref()->dump(b0);
		src->type()->dump(b1);
		b0.append('\0');
		b1.append('\0');
		cg.error(range(), "Cannot assign an rvalue of type '%s' to an lvalue of type '%s'", b1.data(), b0.data());
		return false;
	}
	return dst->store(cg, *src);
}

Bool AstForStmt::codegen(Cg& cg) const noexcept {
	// We always generate a scope outside for this statement since it may have an
	// optional init-stmt which should be scoped to the for block only
	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}
	// <init-stmt>?
	// loop:
	//	cond ? br cond, join, exit : br join
	// join:
	//	<body-stmt>
	//	br post
	// post:
	//	<post-stmt>?
	//	br loop
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
	auto exit_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "exit");

	cg.scopes.last().loop.emplace(post_bb, exit_bb);

	cg.llvm.BuildBr(cg.builder, loop_bb);

	cg.llvm.AppendExistingBasicBlock(this_fn, loop_bb);
	cg.llvm.PositionBuilderAtEnd(cg.builder, loop_bb);
	if (m_expr) {
		auto cond = m_expr->gen_value(cg);
		if (!cond) {
			return false;
		}
		cg.llvm.BuildCondBr(cg.builder, cond->ref(), join_bb, exit_bb);
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

	cg.llvm.PositionBuilderAtEnd(cg.builder, exit_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, exit_bb);

	cg.scopes.pop_back();

	return true;

}

} // namespace Biron