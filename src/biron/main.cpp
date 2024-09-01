#include <stdio.h> // fprintf, stderr
#include <string.h> // strlen, memcpy
#include <stdlib.h> // system

#include <biron/util/allocator.h>
#include <biron/util/file.h>

#include <biron/parser.h>
#include <biron/llvm.h>
#include <biron/cg.h>

using namespace Biron;

namespace Biron {
	// The system interface.
	extern const System SYSTEM_STDC;
}

int main(int argc, char **argv) {
	SystemAllocator allocator{SYSTEM_STDC};

	argc--;
	argv++;
	if (argc == 0) {
		fprintf(stderr, "Usage: %s file.biron\n", argv[-1]);
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
				fprintf(stderr, "Out of memory\n");
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
					fprintf(stderr, "Unknown option %s\n", argv[i]);
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
		fprintf(stderr, "Missing files\n");
		return 1;
	}

	auto llvm = LLVM::load();
	if (!llvm) {
		fprintf(stderr, "Could not load libLLVM\n");
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
			fprintf(stderr, "Unknown source file '%.*s'\n",
				Sint32(filename.length()), filename.data());
			return 1;
		}
		auto file = File::open(SYSTEM_STDC, filename);
		if (!file) {
			fprintf(stderr, "Could not open file: '%.*s'\n",
				Sint32(filename.length()), filename.data());
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
			fprintf(stderr, "Could not parse unit\n");
			return 1;
		}

		auto cg = Cg::make(SYSTEM_STDC, allocator, *llvm, diagnostic);
		if (!cg) {
			fprintf(stderr, "Could not initialize code generator\n");
			return 1;
		}

		if (dump_ast) {
			StringBuilder builder{allocator};
			unit->dump(builder);
			if (builder.valid()) {
				fwrite(builder.data(), builder.length(), 1, stderr);
				fflush(stderr);
			} else {
				return 1;
			}
		}

		if (!unit->codegen(*cg)) {
			return 1;
		}

		auto machine = CgMachine::make(SYSTEM_STDC, *llvm, "x86_64-unknown-none");
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
			fprintf(stderr, "Out of memory\n");
			return 1;
		}

		if (auto name = obj.view(); !cg->emit(*machine, name)) {
			fprintf(stderr, "Could not write object file: '%.*s'\n",
				Sint32(name.length()), name.data());
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
			fprintf(stderr, "Out of memory\n");
			return 1;
		}

		// We should have an executable now.
		if (system(link.data()) != 0) {
			fprintf(stderr, "Could not link executable\n");
			return 1;
		}
	}

	return 0;
}
