# hxptr Makefile
# Builds CLI and TUI variants of Intel HEX memory inspector

# ============================================================================
# Configuration
# ============================================================================

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=gnu99
CFLAGS_DEBUG := -Wall -Wextra -Wpedantic -std=gnu99 -g -DDEBUG
LDFLAGS :=
LIBS := -lm

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := bin
PROFILE_DIR := profiles
EXAMPLE_DIR := examples

# Installation paths
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
DATADIR := $(PREFIX)/share/hxptr
PROFILEDIR := $(DATADIR)/profiles

# Source files
CORE_SOURCES := $(wildcard $(SRC_DIR)/*.c)
CLI_SOURCES := $(wildcard $(SRC_DIR)/cli/*.c)
TUI_SOURCES := $(wildcard $(SRC_DIR)/tui/*.c)

# Object files
CORE_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
CLI_OBJECTS := $(patsubst $(SRC_DIR)/cli/%.c,$(BUILD_DIR)/cli/%.o,$(CLI_SOURCES))
TUI_OBJECTS := $(patsubst $(SRC_DIR)/tui/%.c,$(BUILD_DIR)/tui/%.o,$(TUI_SOURCES))

# Binaries
CLI_BIN := $(BIN_DIR)/hxptr
TUI_BIN := $(BIN_DIR)/hxptr-tui

# Profiles
PROFILES := $(wildcard $(PROFILE_DIR)/*.toml)

# Version
VERSION := 0.1.0

# ============================================================================
# Phony Targets
# ============================================================================

.PHONY: all cli tui clean install uninstall test help debug

# Default target
all: cli

# ============================================================================
# Build Targets
# ============================================================================
#
# AddressSanitizer (memory errors)
asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g
asan: LDFLAGS += -fsanitize=address
asan: clean cli
	@echo "✓ AddressSanitizer build complete"
	@echo "Run with: ASAN_OPTIONS=detect_leaks=1 ./bin/hxptr test.hex"

# UndefinedBehaviorSanitizer (undefined behavior)
ubsan: CFLAGS += -fsanitize=undefined -fno-omit-frame-pointer -g
ubsan: LDFLAGS += -fsanitize=undefined
ubsan: clean cli
	@echo "✓ UndefinedBehaviorSanitizer build complete"

# MemorySanitizer (uninitialized reads) - Clang only
msan: CC = clang
msan: CFLAGS += -fsanitize=memory -fno-omit-frame-pointer -g
msan: LDFLAGS += -fsanitize=memory
msan: clean cli
	@echo "✓ MemorySanitizer build complete"

# ThreadSanitizer (data races) - if you add threading later
tsan: CFLAGS += -fsanitize=thread -fno-omit-frame-pointer -g
tsan: LDFLAGS += -fsanitize=thread
tsan: clean cli
	@echo "✓ ThreadSanitizer build complete"

# All sanitizers (except msan which conflicts)
sanitize-all: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
sanitize-all: LDFLAGS += -fsanitize=address,undefined
sanitize-all: clean cli
	@echo "✓ All sanitizers enabled"
# CLI binary
cli: $(CLI_BIN)


$(CLI_BIN): $(CORE_OBJECTS) $(CLI_OBJECTS) | $(BIN_DIR)
	@echo "Linking CLI binary: $@"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "CLI built successfully: $@"

# TUI binary (placeholder - requires additional setup)
tui: $(TUI_BIN)

$(TUI_BIN): $(CORE_OBJECTS) $(TUI_OBJECTS) | $(BIN_DIR)
	@echo "Linking TUI binary: $@"
	@echo "Note: TUI requires Rust/Go wrapper (not yet implemented)"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "TUI stub built: $@"

# ============================================================================
# Object File Rules
# ============================================================================

# Core object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -I$(INC_DIR) -DVERSION=\"$(VERSION)\" -c $< -o $@

# CLI object files
$(BUILD_DIR)/cli/%.o: $(SRC_DIR)/cli/%.c | $(BUILD_DIR)/cli
	@mkdir -p $(dir $@)
	@echo "Compiling CLI: $<"
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# TUI object files
$(BUILD_DIR)/tui/%.o: $(SRC_DIR)/tui/%.c | $(BUILD_DIR)/tui
	@mkdir -p $(dir $@)
	@echo "Compiling TUI: $<"
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# ============================================================================
# Debug Build
# ============================================================================

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean cli
	@echo "Debug build complete"

# ============================================================================
# Directory Creation
# ============================================================================

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/cli:
	@mkdir -p $(BUILD_DIR)/cli

$(BUILD_DIR)/tui:
	@mkdir -p $(BUILD_DIR)/tui

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# ============================================================================
# Installation
# ============================================================================

install: cli
	@echo "Installing hxptr to $(PREFIX)..."
	install -d $(BINDIR)
	install -m 755 $(CLI_BIN) $(BINDIR)/hxptr
	install -d $(PROFILEDIR)
	@if [ -d $(PROFILE_DIR) ] && [ -n "$(PROFILES)" ]; then \
		install -m 644 $(PROFILES) $(PROFILEDIR)/; \
		echo "✓ Installed $(words $(PROFILES)) profiles to $(PROFILEDIR)"; \
	fi
	@echo "Installation complete"
	@echo ""
	@echo "hxptr installed to: $(BINDIR)/hxptr"
	@echo "Profiles installed to: $(PROFILEDIR)"

install-tui: tui
	@echo "Installing hxptr-tui to $(PREFIX)..."
	install -d $(BINDIR)
	install -m 755 $(TUI_BIN) $(BINDIR)/hxptr-tui
	@echo "TUI installation complete"

uninstall:
	@echo "Uninstalling hxptr..."
	rm -f $(BINDIR)/hxptr
	rm -f $(BINDIR)/hxptr-tui
	rm -rf $(DATADIR)
	@echo "Uninstallation complete"

# ============================================================================
# Testing
# ============================================================================

test: cli
	@echo "Running tests..."
	@if [ -d tests ]; then \
		./tests/run_tests.sh; \
	else \
		echo "No tests directory found"; \
		echo "Create tests/run_tests.sh for automated testing"; \
	fi

# Test with example files
test-examples: cli
	@echo "Testing with example files..."
	@if [ -d $(EXAMPLE_DIR) ] && [ -n "$$(ls -A $(EXAMPLE_DIR)/*.hex 2>/dev/null)" ]; then \
		for hex in $(EXAMPLE_DIR)/*.hex; do \
			echo "Testing: $$hex"; \
			$(CLI_BIN) $$hex --quiet || echo "⚠ Failed: $$hex"; \
		done; \
	else \
		echo "No example hex files found in $(EXAMPLE_DIR)"; \
	fi

# ============================================================================
# Cleaning
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "Clean complete"

# Deep clean (including backup files)
distclean: clean
	@echo "Deep cleaning..."
	find . -name '*~' -delete
	find . -name '*.swp' -delete
	find . -name '.*.swp' -delete
	@echo "Deep clean complete"

# ============================================================================
# Development Helpers
# ============================================================================

# Run CLI with example
run: cli
	@if [ -f $(EXAMPLE_DIR)/firmware.hex ]; then \
		$(CLI_BIN) $(EXAMPLE_DIR)/firmware.hex --profile atmega328p; \
	else \
		echo "No example file at $(EXAMPLE_DIR)/firmware.hex"; \
		echo "Usage: make run HEX=path/to/file.hex"; \
	fi

# Run with custom hex file
run-custom: cli
	@if [ -z "$(HEX)" ]; then \
		echo "Error: Specify hex file with HEX=path/to/file.hex"; \
		exit 1; \
	fi
	$(CLI_BIN) $(HEX) $(ARGS)

test-asan: asan $(TEST_BINS)
	@echo "Running tests with AddressSanitizer..."
	@ASAN_OPTIONS=detect_leaks=1 $(MAKE) test

# Run tests with Valgrind
test-valgrind: debug $(TEST_BINS)
	@echo "Running tests with Valgrind..."
	@for test in $(TEST_BINS); do \
		echo "Testing: $$test"; \
		valgrind --leak-check=full --error-exitcode=1 $$test; \
	done

# Coverage report (requires gcov)
coverage: CFLAGS += --coverage -g
coverage: LDFLAGS += --coverage
coverage: clean test
	@echo "Generating coverage report..."
	@gcov $(SRC_DIR)/*.c
	@echo "Coverage files: *.gcov"
# Valgrind memory check
valgrind: debug
	@if [ -f $(EXAMPLE_DIR)/firmware.hex ]; then \
		valgrind --leak-check=full --show-leak-kinds=all \
			$(CLI_BIN) $(EXAMPLE_DIR)/firmware.hex; \
	else \
		echo "No example file found"; \
	fi

# Static analysis with cppcheck
check:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null || (echo "cppcheck not found"; exit 1)
	cppcheck --enable=all --suppress=missingIncludeSystem \
		-I$(INC_DIR) $(SRC_DIR)/

# Format code (requires clang-format)
format:
	@echo "Formatting code..."
	@which clang-format > /dev/null || (echo "clang-format not found"; exit 1)
	find $(SRC_DIR) $(INC_DIR) -name '*.c' -o -name '*.h' | \
		xargs clang-format -i -style=file
	@echo "Code formatted"

# ============================================================================
# Information
# ============================================================================

# Show build information
info:
	@echo "hxptr v$(VERSION) Build Configuration"
	@echo "====================================="
	@echo "Compiler:     $(CC)"
	@echo "CFLAGS:       $(CFLAGS)"
	@echo "Source dir:   $(SRC_DIR)"
	@echo "Include dir:  $(INC_DIR)"
	@echo "Build dir:    $(BUILD_DIR)"
	@echo "Binary dir:   $(BIN_DIR)"
	@echo "Install prefix: $(PREFIX)"
	@echo ""
	@echo "Core sources: $(words $(CORE_SOURCES))"
	@echo "CLI sources:  $(words $(CLI_SOURCES))"
	@echo "TUI sources:  $(words $(TUI_SOURCES))"
	@echo "Profiles:     $(words $(PROFILES))"

# Help message
help:
	@echo "hxptr v$(VERSION) - Intel HEX Memory Inspector"
	@echo ""
	@echo "Build Targets:"
	@echo "  make              - Build CLI (default)"
	@echo "  make cli          - Build CLI only"
	@echo "  make tui          - Build TUI (requires wrapper)"
	@echo "  make all          - Build everything"
	@echo "  make debug        - Build with debug symbols"
	@echo ""
	@echo "Installation:"
	@echo "  make install      - Install CLI to $(PREFIX)"
	@echo "  make install-tui  - Install TUI to $(PREFIX)"
	@echo "  make uninstall    - Remove installed files"
	@echo ""
	@echo "Testing:"
	@echo "  make test         - Run test suite"
	@echo "  make test-examples - Test with example files"
	@echo "  make valgrind     - Run with memory leak detection"
	@echo "  make check        - Run static analysis"
	@echo ""
	@echo "Development:"
	@echo "  make run          - Run CLI with example file"
	@echo "  make run-custom HEX=file.hex ARGS='--profile atmega328p'"
	@echo "  make format       - Format code with clang-format"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Deep clean (includes backups)"
	@echo ""
	@echo "Information:"
	@echo "  make info         - Show build configuration"
	@echo "  make help         - Show this message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build CLI"
	@echo "  make debug        # Build with debugging"
	@echo "  make install PREFIX=/usr/local"
	@echo "  make run HEX=firmware.hex ARGS='--stats'"

# ============================================================================
# Dependencies
# ============================================================================

# Auto-generate dependencies (optional, for advanced builds)
-include $(CORE_OBJECTS:.o=.d)
-include $(CLI_OBJECTS:.o=.d)
-include $(TUI_OBJECTS:.o=.d)

# Dependency generation
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) -MM -MT '$(@:.d=.o)' $(CFLAGS) -I$(INC_DIR) $< > $@

# ============================================================================
# End of Makefile
# ============================================================================
