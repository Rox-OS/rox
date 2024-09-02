#include <string.h> // strlen, memcpy
#include <stdlib.h> // system

#include <biron/util/allocator.h>
#include <biron/util/file.h>
#include <biron/util/terminal.inl>

#include <biron/parser.h>
#include <biron/llvm.h>
#include <biron/cg.h>

using namespace Biron;

namespace Biron {
	// The system interface.
	extern const System SYSTEM_LINUX;
}

int main(int argc, char **argv) {
	SystemAllocator allocator{SYSTEM_LINUX};
	Terminal terminal{SYSTEM_LINUX};

	argc--;
	argv++;
	if (argc == 0) {
		terminal.err("Usage: %s file.biron\n", argv[-1]);
		return 1;
	}

	Bool bm = false;
	Ulen opt = 0;
	Bool dump_ir = false;
	Bool dump_ast = false;

	Array<StringView> filenames{allocator};
	for (int i = 0; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (!filenames.emplace_back(argv[i], strlen(argv[i]))) {
				terminal.err("Out of memory\n");
				return 1;
			}
		} else if (argv[i][0] == '-') {
			if (argv[i][1] == 'b' && argv[i][2] == 'm') {
				bm = true;
			} else if (argv[i][1] == 'O') {
				switch (argv[i][2]) {
				case '0': opt = 0; break;
				case '1': opt = 1; break;
				case '2': opt = 2; break;
				case '3': opt = 3; break;
				default:
					terminal.err("Unknown option %s\n", argv[i]);
					return 1;
				}
			} else if (argv[i][1] == 'd') {
				if (argv[i][2] == 'a') {
					dump_ast = true;
				} else if (argv[i][2] == 'i') {
					dump_ir = true;
				}
			}
		}
	}

	if (filenames.empty()) {
		terminal.err("Missing files\n");
		return 1;
	}

	auto llvm = LLVM::load(SYSTEM_LINUX);
	if (!llvm) {
		terminal.err("Could not load libLLVM\n");
		return 1;
	}

	// Read in source code of all files
	struct Source {
		StringView  name;
		Array<char> data;
	};
	Array<Source> sources{allocator};
	for (auto filename : filenames) {
		auto dot = filename.find_last_of('.');
		if (!dot) {
			terminal.err("Unknown source file '%S'\n", filename);
			return 1;
		}
		auto file = File::open(SYSTEM_LINUX, filename);
		if (!file) {
			terminal.err("Could not open file: '%S'\n", filename);
			return 1;
		}
		Array<char> data{allocator};
		for (;;) {
			char buffer[4096];
			auto length = file->read(data.length(), buffer, sizeof buffer);
			if (length) {
				auto offset = data.length();
				if (!data.resize(offset + length)) {
					return 1;
				}
				memcpy(data.data() + offset, buffer, length);
			} else {
				break;
			}
		}
		if (!sources.emplace_back(filename, move(data))) {
			return 1;
		}
	}

	// TODO(dweiler): Thread this part
	for (const auto& source : sources) {
		StringView code{source.data.data(), source.data.length()};
		Lexer lexer{source.name, code};
		Diagnostic diagnostic{lexer, allocator};
		Parser parser{lexer, diagnostic, allocator};
		auto unit = parser.parse();
		if (!unit) {
			terminal.err("Could not parse unit\n");
			return 1;
		}

		auto cg = Cg::make(SYSTEM_LINUX, terminal, allocator, *llvm, diagnostic);
		if (!cg) {
			terminal.err("Could not initialize code generator\n");
			return 1;
		}

		if (dump_ast) {
			StringBuilder builder{allocator};
			unit->dump(builder);
			terminal.err(builder.view());
		}

		if (!unit->codegen(*cg)) {
			return 1;
		}

		auto machine = CgMachine::make(terminal, *llvm, "x86_64-unknown-none");
		if (!machine) {
			return 1;
		}

		if (!cg->optimize(*machine, opt)) {
			return 1;
		}

		if (dump_ir && !cg->dump()) {
			return 1;
		}

		// Strip everything up to including '.'
		auto dot = source.name.find_last_of('.');
		auto name = source.name.slice(0, *dot);

		// Build "name.o"
		StringBuilder obj{allocator};
		obj.append(name);
		obj.append('.');
		obj.append('o');
		if (!obj.valid()) {
			terminal.err("Out of memory\n");
			return 1;
		}

		if (auto name = obj.view(); !cg->emit(*machine, name)) {
			terminal.err("Could not write object file: '%S'\n", name);
			return 1;
		}
	}

	if (!bm) {
		// Build "gcc name.o -o name"
		StringBuilder link{allocator};
		link.append("gcc");
		link.append(' ');
		for (const auto& source : sources) {
			auto dot = source.name.find_last_of('.');
			auto name = source.name.slice(0, *dot);
			link.append(name);
			link.append('.');
			link.append('o');
			link.append(' ');
		}
		link.append("-o a.out");
		link.append('\0');
		if (!link.valid()) {
			terminal.err("Out of memory\n");
			return 1;
		}

		// We should have an executable now.
		if (system(link.data()) != 0) {
			terminal.err("Could not link executable\n");
			return 1;
		}
	}

	return 0;
}
