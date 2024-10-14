#include <biron/diagnostic.h>
#include <biron/lexer.h>
#include <biron/util/terminal.inl>

namespace Biron {

void Diagnostic::diagnostic(Range range, Kind kind, StringView message) noexcept {
	// Work out the column and line from the token offset.
	Ulen line_number = 1;
	Ulen this_column = 1;
	// We just count newlines from the beginning of the lexer stream up to but not
	// including where the token starts itself. We also keep track of the previous
	// lines length (last_column) to handle the case where a parse error happens
	// right on the end of a line (see below).
	for (Ulen i = 0; i < range.offset; i++) {
		if (m_lexer[i] == '\n') {
			line_number++;
			this_column = 0;
		}
		this_column++;
	}
	// When the error is right at the end of the line, the above counting logic
	// will report an error on the first column of the next line.
	if (range.offset && m_lexer[range.offset - 1] == '\n') {
		line_number--;
		range.offset--;
	}

	Maybe<Array<char>> msg;
	if (m_terminal.ansi_colors()) {
		msg = format(m_scratch,
		             "\033[1;37m%S:%zu:%zu:\033[0m \033[1;31m%s:\033[0m %S\n",
		             m_lexer.name(),
		             line_number,
		             this_column,
		             kind == Kind::FATAL ? "fatal" : "error",
		             message);
	} else {
		msg = format(m_scratch,
		             "%S:%zu:%zu: %s: %S\n",
		             m_lexer.name(),
		             line_number,
		             this_column,
		             kind == Kind::FATAL ? "fatal" : "error",
		             message);
	}
	if (!msg) {
		return;
	}

	m_terminal.err(StringView { msg->data(), msg->length() });

	// Print the offending line.
	const auto len = m_lexer.data().length();
	if (range.offset == 0 || range.offset >= len) {
		// Do not print the offending line when the error range is invalid.
		return;
	}

	auto line_beg = range.offset;
	auto line_end = range.offset;
	while (line_beg && m_lexer[line_beg - 1] != '\n') {
		line_beg--;
	}
	while (line_end < len && m_lexer[line_end] != '\n') {
		line_end++;
	}
	const auto line_len = line_end - line_beg;

	m_terminal.err(m_lexer.string({ line_beg, line_len }));
	m_terminal.err("\n");

	for (Ulen i = line_beg; i < range.offset; i++) {
		m_terminal.err(" ");
	}
	for (Ulen i = 0; i < range.length; i++) {
		m_terminal.err("~");
	}
	m_terminal.err("\n");
}

} // namespace Biron