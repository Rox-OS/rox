#include <biron/ast_stmt.h>
#include <biron/ast_expr.h>

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

Bool AstBlockStmt::codegen(Cg& cg) const noexcept {
	for (Ulen l = m_stmts.length(), i = 0; i < l; i++) {
		if (!m_stmts[i]->codegen(cg)) {
			return false;
		}
	}
	return true;
}

Bool AstReturnStmt::codegen(Cg& cg) const noexcept {
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
	auto value = m_init->gen_value(cg);
	if (!value) {
		return false;
	}
	auto store = cg.emit_alloca(value->type());
	if (!store) {
		return false;
	}
	if (!store->store(cg, *value)) {
		return false;
	}
	if (!cg.vars.emplace_back(m_name, move(*store))) {
		return false;
	}
	return true;
}

Bool AstExprStmt::codegen(Cg& cg) const noexcept {
	// TODO(dweiler): Optimization to omit the value
	auto value = m_expr->gen_value(cg);
	if (!value) {
		fprintf(stderr, "Could not generate expression\n");
		return false;
	}
	return true;
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
	return dst->store(cg, *src);
}

Bool AstForStmt::codegen(Cg& cg) const noexcept {
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

	if (!cg.loops.emplace_back(post_bb, exit_bb)) {
		return false;
	}

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

	join_bb = cg.llvm.GetInsertBlock(cg.builder);
	cg.llvm.BuildBr(cg.builder, post_bb);

	cg.llvm.PositionBuilderAtEnd(cg.builder, post_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, post_bb);
	if (m_post && !m_post->codegen(cg)) {
		return false;
	}

	post_bb = cg.llvm.GetInsertBlock(cg.builder);
	cg.llvm.BuildBr(cg.builder, loop_bb);

	cg.llvm.PositionBuilderAtEnd(cg.builder, exit_bb);
	cg.llvm.AppendExistingBasicBlock(this_fn, exit_bb);

	cg.loops.pop_back();

	return true;

}

} // namespace Biron