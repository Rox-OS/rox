#include <biron/util/allocator.inl>

#include <biron/parser.h>
#include <biron/llvm.h>
#include <biron/codegen.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace Biron;

namespace Biron {

struct Mallocator : Allocator {
	constexpr Mallocator() noexcept
		: temporary{*this}
	{
	}
	~Mallocator() {
		for (Ulen l = temporary.length(), i = 0; i < l; i++) {
			deallocate(temporary[i], 0);
		}
	}
	virtual void* allocate(Ulen size) noexcept override {
		return malloc(size);
	}
	virtual void* scratch(Ulen size) noexcept override {
		Ulen offset = temporary.length();
		if (!temporary.reserve(offset + 1)) {
			return nullptr;
		}
		void* ptr = allocate(size);
		if (!ptr) {
			return nullptr;
		}
		(void)temporary.push_back(ptr); // Cannot fail
		return ptr;
	}
	virtual void deallocate(void* old, Ulen size) noexcept override {
		(void)size;
		free(old);
	}
	Array<void*> temporary;
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
	int file = -1;
	for (int i = 0; i < argc; i++) {
		if (argv[i][0] != '-' && file == -1) {
			file = i;
		} else if (argv[i][0] == '-' && argv[i][1] == 'b' && argv[i][2] == 'm') {
			bm = true;
		}
	}

	if (file == -1) {
		fprintf(stderr, "Missing filename\n");
		return 1;
	}

	auto llvm = LLVM::load();
	if (!llvm) {
		fprintf(stderr, "Could not load libLLVM\n");
		return 1;
	}

	// Load in file
	FILE *fp = fopen(argv[file], "rb");
	if (!fp) {
		fprintf(stderr, "Could not open '%s'\n", argv[file]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	Ulen n = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	Array<char> src{mallocator};
	if (!src.resize(n)) {
		fprintf(stderr, "Out of memory\n");
		fclose(fp);
		return 1;
	}
	if (fread(src.data(), n, 1, fp) != 1) {
		fprintf(stderr, "Could not read '%s'\n", argv[file]);
		fclose(fp);
		return 1;
	}
	fclose(fp);

	StringView name{argv[file], strlen(argv[file])};
	auto dot = name.find_last_of('.');
	if (!dot) {
		fprintf(stderr, "Unknown source file: %s\n", argv[file]);
		return 1;
	}

	StringView code{src.data(), src.length()};
	Parser parser{name, code, mallocator};
	auto unit = parser.parse();
	if (!unit) {
		return 1;
	}

	StringBuilder builder{mallocator};
	if (false && unit->dump(builder)) {
		auto view = builder.view();
		printf("%.*s\n", (int)view.length(), view.data());
	}

	Codegen gen{*llvm, mallocator, "x86_64-linux-pc-unknown"};
	if (!gen.run(*unit)) {
		return 1;
	}

	if (false) {
		gen.dump();
	}

	if (!gen.optimize()) {
		return 1;
	}

	if (false) {
		gen.dump();
	}

	// Strip everything up to including '.'
	name = name.slice(0, *dot);

	// Build "name.o"
	StringBuilder obj{mallocator};
	obj.append(name);
	obj.append('.');
	obj.append('o');
	if (!obj.valid()) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	if (!gen.emit(obj.view())) {
		return 1;
	}

	if (!bm) {
		// Build "gcc name.o -o name"
		StringBuilder link{mallocator};
		link.append("gcc");
		link.append(' ');
		link.append(obj.view());
		link.append(' ');
		link.append("-o");
		link.append(name);
		link.append('\0');
		if (!link.valid()) {
			fprintf(stderr, "Out of memory\n");
			return false;
		}

		// We should have an executable now.
		if (system(link.data()) != 0) {
			fprintf(stderr, "Could not link executable\n");
			return 1;
		}
	}

	return 0;
}
