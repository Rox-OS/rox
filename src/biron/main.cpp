#include <stdio.h> // fprintf, fopen, fclose, fseek, ftell, fread, stderr
#include <string.h> // strlen
#include <stdlib.h> // malloc, free, system

#include <biron/util/allocator.inl>

#include <biron/parser.h>
#include <biron/llvm.h>
#include <biron/cg.h>

using namespace Biron;

namespace Biron {

struct Mallocator : Allocator {
	constexpr Mallocator() noexcept = default;
	virtual void* allocate(Ulen size) noexcept override {
		return malloc(size);
	}
	virtual void deallocate(void* old, Ulen size) noexcept override {
		(void)size;
		free(old);
	}
};

} // namespace Biron

int main(int argc, char **argv) {
	Mallocator mallocator;

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

	Array<StringView> filenames{mallocator};
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
	struct File {
		StringView  name;
		Array<char> data;
	};

	Array<File> files{mallocator};
	for (auto& filename : filenames) {
		auto dot = filename.find_last_of('.');
		if (!dot) {
			fprintf(stderr, "Unknown source file '%.*s'\n",
				Sint32(filename.length()), filename.data());
			return 1;
		}
		FILE* fp = filename == "-" ? stdin : fopen(filename.terminated(mallocator), "rb");
		if (!fp) {
			fprintf(stderr, "Could not open '%.*s'\n",
				Sint32(filename.length()), filename.data());
			return false;
		}
		Array<char> src{mallocator};
		if (fseek(fp, 0, SEEK_END) != 0) {
			while (!feof(fp)) {
				if (!src.push_back(fgetc(fp))) {
					goto L_oom;
				}
			}
		} else {
			const auto tell = ftell(fp);
			if (tell <= 0) {
				goto L_error;
			}
			if (!src.resize(tell)) {
				goto L_oom;
			}
			if (fseek(fp, 0, SEEK_SET) != 0) {
				goto L_error;
			}
			if (fread(src.data(), src.length(), 1, fp) != 1) {
				goto L_error;
			}
		}
		if (!files.emplace_back(filename, move(src))) {
			goto L_oom;
		}
		continue;
	L_oom:
		fprintf(stderr, "Out of memory\n");
		goto L_exit;
	L_error:
		fprintf(stderr, "Could not read '%.*s'",
			Sint32(filename.length()), filename.data());
		goto L_exit;
	L_exit:
		if (fp != stdin) {
			fclose(fp);
		}
		return 1;
	}

	// TODO(dweiler): Thread this part
	for (const auto& file : files) {
		StringView code{file.data.data(), file.data.length()};
		Lexer lexer{file.name, code};
		Diagnostic diagnostic{lexer, mallocator};
		Parser parser{lexer, diagnostic, mallocator};
		auto unit = parser.parse();
		if (!unit) {
			fprintf(stderr, "Could not parse unit\n");
			return 1;
		}

		auto cg = Cg::make(mallocator, *llvm, diagnostic);
		if (!cg) {
			fprintf(stderr, "Could not initialize code generator\n");
			return 1;
		}

		if (dump_ast) {
			StringBuilder builder{mallocator};
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

		auto machine = CgMachine::make(*llvm, "x86_64-unknown-none");
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
		auto dot = file.name.find_last_of('.');
		auto name = file.name.slice(0, *dot);

		// Build "name.o"
		StringBuilder obj{mallocator};
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
		StringBuilder link{mallocator};
		link.append("gcc");
		link.append(' ');
		for (const auto& file : files) {
			auto dot = file.name.find_last_of('.');
			auto name = file.name.slice(0, *dot);
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
