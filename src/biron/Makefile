LTO ?= 0
ASAN ?= 0
TSAN ?= 0
UBSAN ?= 0
DEBUG ?= 0
UNUSED ?= 1

BIN := bironc

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

CC := gcc
CC ?= clang

# We use the C frontend with -xc++ to avoid linking in C++ runtime library.
CXX := $(CC) -xc++

# Determine if the C or C++ compiler should be used as the linker frontend.
ifneq (,$(findstring -xc++,$(CXX)))
	LD := $(CC)
else
	LD := $(CXX)
endif

# Determine the build type.
ifeq ($(DEBUG),1)
	TYPE := debug
else
	TYPE := release
endif

# Build artifact directories.
OBJDIR := .build/$(TYPE)/objs
DEPDIR := .build/$(TYPE)/deps

# Collect all .cpp, files for build in the source directory.
SRCS := $(call rwildcard, , *cpp)

# Generate object and dependency filenames.
OBJS := $(filter %.o,$(SRCS:%.cpp=$(OBJDIR)/%.o))

DEPS := $(filter %.d,$(SRCS:%.cpp=$(DEPDIR)/%.d))
DEPS += $(filter %.d,$(SRCS:%.c=$(DEPDIR)/%.d))

#
# Dependency flags.
#
DEPFLAGS := -MMD
DEPFLAGS += -MP

#
# C++ compilation flags.
#
CXXFLAGS := -I..
CXXFLAGS += -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
CXXFLAGS += -std=c++20

# Give each function and data it's own section so the linker can remove unused
# references to each, producing smaller, tighter binaries.
ifeq ($(UNUSED),1)
	CXXFLAGS += -ffunction-sections
	CXXFLAGS += -fdata-sections
endif

# Disable unneded features in release builds.
ifneq ($(DEBUG),1)
	# These are needed for stack traces in debug builds.
	CXXFLAGS += -fno-unwind-tables
	CXXFLAGS += -fno-asynchronous-unwind-tables
endif

# Enable link-time optimizations if requested.
ifeq ($(LTO),1)
	CXXFLAGS += -flto
endif

ifeq ($(DEBUG),1)
	CXXFLAGS += -g

	# Optimize for debugging.
	CXXFLAGS += -O0

	# Ensure there's a frame pointer in debug builds.
	CXXFLAGS += -fno-omit-frame-pointer
else
	# Disable C assertions.
	CXXFLAGS += -DNDEBUG

	# Highest optimization flag for release builds.
	CXXFLAGS += -O3

	# Disable all stack protection features.
	CXXFLAGS += -fno-stack-protector
	CXXFLAGS += -fno-stack-check
	ifeq ($(CC),gcc)
		CXXFLAGS += -fno-stack-clash-protection
	endif

	# Disable frame pointer too except when using ASAN.
	ifeq ($(ASAN),1)
		CXXFLAGS += -fno-omit-frame-pointer
	else
		CXXFLAGS += -fomit-frame-pointer
	endif
endif

# Sanitizer selection.
ifeq ($(ASAN),1)
	CXXFLAGS += -fsanitize=address
endif
ifeq ($(TSAN),1)
	CXXFLAGS += -fsanitize=thread
endif
ifeq ($(UBSAN),1)
	CXXFLAGS += -fsanitize=undefined
endif

#
# Linker flags
#
LDFLAGS := -ldl

# Strip unused symbols if requested.
ifeq ($(UNUSED), 1)
	LDFLAGS += -Wl,--gc-sections
endif

# Enable link-time optimizations if requested.
ifeq ($(LTO),1)
	LDFLAGS += -flto
endif

# Sanitizer selection.
ifeq ($(ASAN),1)
	LDFLAGS += -fsanitize=address
endif
ifeq ($(TSAN),1)
	LDFLAGS += -fsanitize=thread
endif
ifeq ($(UBSAN),1)
	LDFLAGS += -fsanitize=undefined
endif

# Strip the binary when not a debug build.
ifeq ($(DEBUG),1)
	STRIP := true
else
	STRIP := strip
endif

all: $(BIN)

# Build artifact directories.
$(DEPDIR):
	@mkdir -p $(addprefix $(DEPDIR)/,$(call uniq,$(dir $(SRCS))))

$(OBJDIR):
	@mkdir -p $(addprefix $(OBJDIR)/,$(call uniq,$(dir $(SRCS))))

$(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.d | $(OBJDIR) $(DEPDIR)
	$(CXX) -MT $@ $(DEPFLAGS) -MF $(DEPDIR)/$*.Td $(CXXFLAGS) -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

$(BIN): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $@ $(OFLAGS)
	$(STRIP) $@

clean:
	rm -rf $(BIN)
	rm -rf $(DEPDIR)
	rm -rf $(OBJDIR)

purge:
	rm -rf .build

.PHONY: $(DEPDIR) $(OBJDIR) clean purge

$(DEPS):
include $(wildcard $)