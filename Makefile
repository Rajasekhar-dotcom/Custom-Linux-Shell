# =============================================================================
# Makefile — Mini-Bash build system
#
# Usage
#   make           Build the shell (default target)
#   make clean     Remove compiled objects and the binary
#   make debug     Build with debug symbols and AddressSanitizer
#   make valgrind  Run under Valgrind to detect memory leaks
# =============================================================================

# ── Toolchain ─────────────────────────────────────────────────────────────────
CC      := gcc
TARGET  := minibash

# ── Compiler flags ────────────────────────────────────────────────────────────
#   -std=c11          Use C11 standard (getline, strtok, etc.)
#   -Wall             Enable all common warnings
#   -Wextra           Enable extra warnings (unused vars, missing return, …)
#   -Wpedantic        Enforce strict ISO C compliance
#   -D_POSIX_C_SOURCE=200809L  Expose POSIX.1-2008 APIs (getline, strtok_r, …)
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L

# Debug flags (used by 'make debug')
DEBUG_FLAGS := -g3 -fsanitize=address,undefined -fno-omit-frame-pointer

# ── Source files ──────────────────────────────────────────────────────────────
SRCS    := main.c shell.c parser.c executor.c builtins.c utils.c
OBJS    := $(SRCS:.c=.o)
HDRS    := shell.h

# ── Default target ────────────────────────────────────────────────────────────
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo ""
	@echo "  Build successful → ./$(TARGET)"
	@echo ""

# Pattern rule: compile each .c into a .o
# $<  = the .c dependency  |  $@  = the .o target
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Debug build ───────────────────────────────────────────────────────────────
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# ── Valgrind memory check ─────────────────────────────────────────────────────
.PHONY: valgrind
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --verbose ./$(TARGET)

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned."
