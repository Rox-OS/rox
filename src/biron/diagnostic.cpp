#include <stdio.h> // fprintf, stderr

#include <biron/diagnostic.h>
#include <biron/lexer.h>

namespace Biron {

void Diagnostic::diagnostic(Range range, const char *message) noexcept {
	// Work out the column and line from the token offset.
	Ulen line_number = 1;
	Ulen this_column = 1;
	Ulen last_column = 1;
	// We just count newlines from the beginning of the lexer stream up to but not
	// including where the token starts itself. We also keep track of the previous
	// lines length (last_column) to handle the case where a parse error happens
	// right on the end of a line (see below).
	for (Ulen i = 0; i < range.offset; i++) {
		if (m_lexer[i] == '\n') {
			line_number++;
			last_column = this_column;
			this_column = 0;
		}
		this_column++;
	}
	// When the error is right at the end of the line, the above counting logic
	// will report an error on the first column of the next line.
	if (this_column == 1 && line_number > 1) {
		line_number--;
		this_column = last_column;
	}
	fprintf(stderr, "\033[1;37m%.*s:%zu:%zu:\033[0m \033[1;31merror:\033[0m %s\n",
	        (int)m_lexer.name().length(),
	        m_lexer.name().data(),
	        line_number,
	        this_column,
	        message);

	// Print the offending line.
	auto line_beg = range.offset;
	auto line_end = range.offset;
	while (line_beg && m_lexer[line_beg - 1] != '\n') {
		line_beg--;
	}
	while (m_lexer[line_end + 1] != '\n') {
		line_end++;
	}
	const auto line_len = line_end - line_beg;
	fprintf(stderr, "%.*s\n", Sint32(line_len), &m_lexer[line_beg]);

	// Then print some swiggles underneath the offense.
	for (Ulen i = line_beg; i < range.offset; i++) {
		fprintf(stderr, " ");
	}
	for (Ulen i = 0; i < range.length; i++) {
		fprintf(stderr, "~");
	}
	fprintf(stderr, "\n");
}

} // namespace Biron