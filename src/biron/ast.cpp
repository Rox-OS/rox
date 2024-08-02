#include <biron/ast.h>
#include <biron/codegen.h>

namespace Biron {

Maybe<Value> AstExpr::address(Codegen& gen) noexcept {
	StringBuilder builder{gen.allocator};
	dump(builder);
	auto view = builder.view();
	fprintf(stderr, "Unsupported address on: %.*s\n", (int)view.length(), view.data());
	return None{};
}

Maybe<Value> AstExpr::codegen(Codegen& gen) noexcept {
	StringBuilder builder{gen.allocator};
	dump(builder);
	auto view = builder.view();
	fprintf(stderr, "Unsupported codegen on: %.*s\n", (int)view.length(), view.data());
	return None{};
}

void AstTupleExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	for (Ulen l = exprs.length(), i = 0; i < l; i++) {
		exprs[i]->dump(builder);
		if (i + 1 != l) {
			builder.append(", ");
		}
	}
	builder.append(')');
}

void AstCallExpr::dump(StringBuilder& builder) const noexcept {
	callee->dump(builder);
	if (args) {
		args->dump(builder);
	} else {
		builder.append("()");
	}
}

void AstTypeExpr::dump(StringBuilder& builder) const noexcept {
	type->dump(builder);
}

Maybe<Value> AstTypeExpr::codegen(Codegen&) noexcept {
	return None{};
}

void AstVarExpr::dump(StringBuilder& builder) const noexcept {
	builder.append(name);
}

void AstIntExpr::dump(StringBuilder& builder) const noexcept {
	builder.append(value);
}

void AstStrExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('"');
	builder.append(literal);
	builder.append('"');
}

void AstBinExpr::dump(StringBuilder& builder) const noexcept {
	lhs->dump(builder);
	switch (op) {
	/****/ case Operator::ADD:    builder.append(" + ");
	break; case Operator::SUB:    builder.append(" - ");
	break; case Operator::MUL:    builder.append(" * ");
	break; case Operator::EQEQ:   builder.append(" == ");
	break; case Operator::NEQ:    builder.append(" != ");
	break; case Operator::GT:     builder.append(" > ");
	break; case Operator::GTE:    builder.append(" >= ");
	break; case Operator::LT:     builder.append(" < ");
	break; case Operator::LTE:    builder.append(" <= ");
	break; case Operator::AS:     builder.append(" as ");
	break; case Operator::LOR:    builder.append(" || ");
	break; case Operator::LAND:   builder.append(" && ");
	break; case Operator::BOR:    builder.append(" | ");
	break; case Operator::BAND:   builder.append(" && ");
	break; case Operator::LSHIFT: builder.append(" << ");
	break; case Operator::RSHIFT: builder.append(" >> ");
	break; case Operator::DOT:    builder.append('.');
	break; case Operator::OF:     builder.append(" of ");
	break;
	}
	rhs->dump(builder);
}

void AstUnaryExpr::dump(StringBuilder& builder) const noexcept {
	switch (op) {
	/****/ case Operator::NEG:    builder.append('-');
	break; case Operator::NOT:    builder.append('!');
	break; case Operator::DEREF:  builder.append('*');
	break; case Operator::ADDROF: builder.append('&');
	}
	operand->dump(builder);
}

void AstIndexExpr::dump(StringBuilder& builder) const noexcept {
	operand->dump(builder);
	builder.append('[');
	index->dump(builder);
	builder.append(']');
}

void AstAsmRegExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('%');
	builder.append(name);
	if (segment) {
		builder.append(':');
		segment->dump(builder);
	}
};

void AstAsmImmExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('$');
	if (neg) builder.append('-');
	expr->dump(builder);
}

void AstAsmMemExpr::dump(StringBuilder& builder) const noexcept {
	if (base) {
		base->dump(builder);
	}
	builder.append('(');
	if (offset) {
		offset->dump(builder);
		builder.append(", ");
		index->dump(builder);
		builder.append(", ");
		size->dump(builder);
	}
	builder.append(')');
}

void AstAsmSubExpr::dump(StringBuilder& builder) const noexcept {
	builder.append('$');
	builder.append('(');
	builder.append(selector);
	builder.append('.');
	builder.append(field);
	if (modifier) {
		builder.append(':');
		builder.append(*modifier);
	}
	builder.append(')');
}

void AstTupleType::dump(StringBuilder& builder) const noexcept {
	builder.append('(');
	for (Ulen l = elems.length(), i = 0; i < l; i++) {
		const auto &elem = elems[i];
		if (elem.name) {
			builder.append(*elem.name);
		} else {
			builder.append(i);
		}
		builder.append(':');
		builder.append(' ');
		elem.type->dump(builder);
		if (i + 1 != l) {
			builder.append(", ");
		}
	}
	builder.append(')');
}

void AstIdentType::dump(StringBuilder& builder) const noexcept {
	builder.append(ident);
}

void AstVarArgsType::dump(StringBuilder& builder) const noexcept {
	builder.repeat('.', 3);
}

void AstPtrType::dump(StringBuilder& builder) const noexcept {
	builder.append('*');
	type->dump(builder);
}

void AstArrayType::dump(StringBuilder& builder) const noexcept {
	builder.append('[');
	extent->dump(builder);
	builder.append(']');
	type->dump(builder);
}

void AstSliceType::dump(StringBuilder& builder) const noexcept {
	builder.append("[]");
	type->dump(builder);
}

void AstBlockStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.append('\n');
	builder.repeat('\t', depth);
	builder.append("{\n");
	for (Ulen l = stmts.length(), i = 0; i < l; i++) {
		stmts[i]->dump(builder, depth + 1);
	}
	builder.repeat('\t', depth);
	builder.append("}\n");
}

void AstReturnStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("return");
	builder.append(' ');
	if (expr) {
		expr->dump(builder);
	}
	builder.append(';');
	builder.append('\n');
}

void AstDeferStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("defer");
	builder.append('\n');
	builder.repeat('\t', depth);
	builder.append("{\n");
	stmt->dump(builder, depth + 1);
	builder.repeat('\t', depth);
	builder.append("}\n");
}

void AstIfStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("if");
	builder.append(' ');
	if (init) {
		init->dump(builder, 0);
		builder.pop();
		builder.append(' ');
	}
	expr->dump(builder);
	builder.repeat('\t', depth);
	then->dump(builder, depth);
	if (elif) {
		builder.repeat('\t', depth);
		builder.append("else");
		builder.append(' ');
		elif->dump(builder, depth);
	}
}

void AstLetStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("let");
	builder.append(' ');
	builder.append(name);
	if (init) {
		builder.append(" = ");
		init->dump(builder);
	}
	builder.append(';');
	builder.append('\n');
}

void AstForStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append("for");
	builder.append(' ');
	if (init) {
		init->dump(builder, 0);
		builder.pop();
		builder.append(' ');
	}
	expr->dump(builder);
	if (post) {
		builder.append(';');
		builder.append(' ');
		post->dump(builder, 0);
		builder.pop(); // Remove '\n'
		builder.pop(); // Remove ';'
	}
	body->dump(builder, depth);
}

void AstExprStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	expr->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstAssignStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	dst->dump(builder);
	builder.append(" = ");
	src->dump(builder);
	builder.append(';');
	builder.append('\n');
}

void AstAsmStmt::dump(StringBuilder& builder, int depth) const noexcept {
	builder.repeat('\t', depth);
	builder.append(mnemonic);
	for (Ulen l = operands.length(), i = 0; i < l; i++) {
		operands[i]->dump(builder);
		if (i + 1 != l) {
			builder.append(", ");
		}
	}
	builder.append(';');
	builder.append('\n');
}

void AstFn::dump(StringBuilder& builder, int depth) const noexcept {
	builder.append("fn");
	if (generic) {
		generic->dump(builder);
	}
	builder.append(' ');
	builder.append(name);
	if (type) {
		type->dump(builder);
	} else {
		builder.append("()");
	}
	builder.append(" -> ");
	if (rtype) {
		rtype->dump(builder);
	} else {
		builder.append("Unit");
	}
	body->dump(builder, depth);
}

void AstAsm::dump(StringBuilder& builder) const noexcept {
	builder.append("asm");
	builder.append(' ');
	builder.append(name);
	type->dump(builder);
	if (clobbers) {
		builder.append(" -> ");
		clobbers->dump(builder);
	}
	builder.append("\n");
	builder.append("{\n");
	for (Ulen l = stmts.length(), i = 0; i < l; i++) {
		stmts[i]->dump(builder, 1);
	}
	builder.append("}\n");
}

} // namespace Biron