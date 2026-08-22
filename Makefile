# hxptr Makefile Intel HEX memory inspector (CLI + TUI)

CC       := gcc
CFLAGS   := -Wall -Wextra -Wpedantic -std=gnu99 -Os
LDFLAGS  :=
LIBS     := -lm
VERSION  := 0.1.0

SRC_DIR      := src
INC_DIR      := include
BUILD_DIR    := build
BIN_DIR      := bin
PROFILE_DIR  := profiles
EXAMPLE_DIR  := examples

PREFIX      ?= /usr/local
BINDIR      := $(PREFIX)/bin
DATADIR     := $(PREFIX)/share/hxptr
PROFILEDIR  := $(DATADIR)/profiles

CORE_SOURCES := $(wildcard $(SRC_DIR)/*.c)
CLI_SOURCES  := $(wildcard $(SRC_DIR)/cli/*.c)
TUI_SOURCES  := $(wildcard $(SRC_DIR)/tui/*.c)

CORE_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
CLI_OBJECTS  := $(patsubst $(SRC_DIR)/cli/%.c,$(BUILD_DIR)/cli/%.o,$(CLI_SOURCES))
TUI_OBJECTS  := $(patsubst $(SRC_DIR)/tui/%.c,$(BUILD_DIR)/tui/%.o,$(TUI_SOURCES))

CLI_BIN  := $(BIN_DIR)/hxptr
TUI_BIN  := $(BIN_DIR)/hxptr-tui
PROFILES := $(wildcard $(PROFILE_DIR)/*.toml)

.PHONY: all cli tui debug sanitize install install-tui uninstall \
        test test-examples run run-custom valgrind check format \
        clean distclean info help \
        fuzz fuzz-record fuzz-loadfile fuzz-profile fuzz-hxql fuzz-tokenizer fuzz-clean

all: cli

# ---------------------------------------------------------------- build ---

cli: $(CLI_BIN)

$(CLI_BIN): $(CORE_OBJECTS) $(CLI_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Built: $@"

# TUI: placeholder until the Rust/Go wrapper lands
tui: $(TUI_BIN)

$(TUI_BIN): $(CORE_OBJECTS) $(TUI_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Built (stub): $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(INC_DIR) -DVERSION=\"$(VERSION)\" -c $< -o $@

$(BUILD_DIR)/cli/%.o: $(SRC_DIR)/cli/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/tui/%.o: $(SRC_DIR)/tui/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

debug: CFLAGS += -g -DDEBUG
debug: clean cli
	@echo "Debug build complete"

# make sanitize SAN=address   (or undefined, thread, memory, address,undefined ...)
SAN ?= address,undefined
sanitize: clean
	$(MAKE) cli CC=$(CC) \
		CFLAGS="$(CFLAGS) -fsanitize=$(SAN) -fno-omit-frame-pointer -g" \
		LDFLAGS="$(LDFLAGS) -fsanitize=$(SAN)"
	@echo "Built with -fsanitize=$(SAN)"
	@echo "Tip: for memory sanitizer use CC=clang; for leak checks: ASAN_OPTIONS=detect_leaks=1"

# ------------------------------------------------------------ install ---

install: cli
	install -d $(BINDIR) $(PROFILEDIR)
	install -m 755 $(CLI_BIN) $(BINDIR)/hxptr
	@if [ -n "$(PROFILES)" ]; then \
		install -m 644 $(PROFILES) $(PROFILEDIR)/; \
		echo "Installed $(words $(PROFILES)) profiles to $(PROFILEDIR)"; \
	fi
	@echo "hxptr installed to $(BINDIR)/hxptr"

install-tui: tui
	install -d $(BINDIR)
	install -m 755 $(TUI_BIN) $(BINDIR)/hxptr-tui

uninstall:
	rm -f $(BINDIR)/hxptr $(BINDIR)/hxptr-tui
	rm -rf $(DATADIR)

# --------------------------------------------------------------- test ---

test: cli
	@if [ -d tests ]; then ./tests/run_tests.sh; \
	else echo "No tests directory found (create tests/run_tests.sh)"; fi

test-examples: cli
	@if [ -d $(EXAMPLE_DIR) ] && ls $(EXAMPLE_DIR)/*.hex >/dev/null 2>&1; then \
		for hex in $(EXAMPLE_DIR)/*.hex; do \
			echo "Testing: $$hex"; \
			$(CLI_BIN) $$hex --quiet || echo "FAILED: $$hex"; \
		done; \
	else echo "No example hex files found in $(EXAMPLE_DIR)"; fi

valgrind: debug
	@if [ -f $(EXAMPLE_DIR)/firmware.hex ]; then \
		valgrind --leak-check=full --show-leak-kinds=all $(CLI_BIN) $(EXAMPLE_DIR)/firmware.hex; \
	else echo "No example file found"; fi

format:
	@which clang-format > /dev/null || (echo "clang-format not found"; exit 1)
	find $(SRC_DIR) $(INC_DIR) \( -name '*.c' -o -name '*.h' \) -exec clang-format -i -style=file {} +

# --------------------------------------------------------------- dev ---

run: cli
	@if [ -f $(EXAMPLE_DIR)/firmware.hex ]; then \
		$(CLI_BIN) $(EXAMPLE_DIR)/firmware.hex --profile atmega328p; \
	else echo "No example file at $(EXAMPLE_DIR)/firmware.hex — try: make run-custom HEX=path/to/file.hex"; fi

run-custom: cli
	@if [ -z "$(HEX)" ]; then echo "Error: Specify HEX=path/to/file.hex"; exit 1; fi
	$(CLI_BIN) $(HEX) $(ARGS)

# --------------------------------------------------------------- fuzz ---
# Harnesses live in fuzz/*.c, one function per target under test:
#   fuzz_record    - hex_line_to_record()   (in-memory, fastest)
#   fuzz_loadfile  - hxptr_load_file()       (full parse + memory-growth path)
#   fuzz_profile   - mcu_profile_load()      (separate .prof text format)
#   fuzz_hxql      - hxql_parse()            (query language, fixed fixture)
#   fuzz_tokenizer - get_token()             (REPL tokenizer)
#
# libFuzzer mode (needs clang): make fuzz FUZZ_CC=clang
# Then run e.g.:  bin/fuzz/fuzz_record -max_len=4096 fuzz/corpus/record
#
# No clang? Each harness also builds as a plain binary (fuzz/driver.c
# supplies main()) that takes one file argument, so it drops straight
# into classic AFL/afl-gcc: afl-fuzz -i corpus -o out -- bin/fuzz/fuzz_record @@

FUZZ_DIR       := fuzz
FUZZ_BIN_DIR   := $(BIN_DIR)/fuzz
FUZZ_CC        ?= clang
FUZZ_SAN       ?= address,undefined
FUZZ_CFLAGS    := -g -O1 -I$(INC_DIR) -fno-omit-frame-pointer

CLI_SOURCES_NO_MAIN := $(filter-out $(SRC_DIR)/cli/main.c,$(CLI_SOURCES))
FUZZ_TARGET_SOURCES := $(CORE_SOURCES) $(CLI_SOURCES_NO_MAIN)
FUZZ_HARNESSES       := record loadfile profile hxql tokenizer

# libFuzzer build: harness provides LLVMFuzzerTestOneInput, clang supplies main via -fsanitize=fuzzer
$(FUZZ_BIN_DIR)/fuzz_%: $(FUZZ_DIR)/fuzz_%.c $(FUZZ_TARGET_SOURCES)
	@mkdir -p $(@D)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -fsanitize=fuzzer,$(FUZZ_SAN) \
		-o $@ $< $(FUZZ_TARGET_SOURCES)

fuzz-record:    $(FUZZ_BIN_DIR)/fuzz_record
fuzz-loadfile:  $(FUZZ_BIN_DIR)/fuzz_loadfile
fuzz-profile:   $(FUZZ_BIN_DIR)/fuzz_profile
fuzz-hxql:      $(FUZZ_BIN_DIR)/fuzz_hxql
fuzz-tokenizer: $(FUZZ_BIN_DIR)/fuzz_tokenizer

fuzz: $(addprefix fuzz-,$(FUZZ_HARNESSES))
	@echo "Built: $(foreach h,$(FUZZ_HARNESSES),$(FUZZ_BIN_DIR)/fuzz_$(h))"
	@echo "Run e.g.: $(FUZZ_BIN_DIR)/fuzz_record fuzz/corpus/record -max_len=4096"

# Classic-AFL / gcc fallback build (no clang, no libFuzzer): links fuzz/driver.c
# as main() instead, producing a binary that takes one filename argument.
fuzz-afl-%: $(FUZZ_DIR)/fuzz_%.c $(FUZZ_DIR)/driver.c $(FUZZ_TARGET_SOURCES)
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) -g -fsanitize=$(FUZZ_SAN) -I$(INC_DIR) \
		-o $(FUZZ_BIN_DIR)/fuzz_$*_afl $(FUZZ_DIR)/fuzz_$*.c $(FUZZ_DIR)/driver.c $(FUZZ_TARGET_SOURCES)

fuzz-clean:
	rm -rf $(FUZZ_BIN_DIR)

# ------------------------------------------------------------- clean ---

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

distclean: clean fuzz-clean
	find . \( -name '*~' -o -name '*.swp' -o -name '.*.swp' \) -delete

# --------------------------------------------------------------- info ---

info:
	@echo "hxptr v$(VERSION)"
	@echo "CC=$(CC)  CFLAGS=$(CFLAGS)"
	@echo "Core sources: $(words $(CORE_SOURCES))  CLI: $(words $(CLI_SOURCES))  TUI: $(words $(TUI_SOURCES))  Profiles: $(words $(PROFILES))"
	@echo "Install prefix: $(PREFIX)"

help:
	@echo "hxptr v$(VERSION) - Intel HEX Memory Inspector"
	@echo ""
	@echo "build:      make [cli|tui|all|debug]"
	@echo "sanitize:   make sanitize SAN=address   (or undefined, thread, memory, address,undefined)"
	@echo "install:    make install | install-tui | uninstall  [PREFIX=/usr/local]"
	@echo "test:       make test | test-examples | valgrind | coverage | check"
	@echo "dev:        make run | run-custom HEX=file.hex ARGS='--stats' | format"
	@echo "fuzz:       make fuzz [FUZZ_CC=clang] [FUZZ_SAN=address,undefined]  (or fuzz-record, fuzz-afl-record, etc.)"
	@echo "clean:      make clean | distclean | fuzz-clean"
	@echo "info:       make info | help"

# ------------------------------------------------------------ depends ---

-include $(CORE_OBJECTS:.o=.d) $(CLI_OBJECTS:.o=.d) $(TUI_OBJECTS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) -MM -MT '$(@:.d=.o)' $(CFLAGS) -I$(INC_DIR) $< > $@
