# yamlget Makefile

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin

# ── Main binary ───────────────────────────────────────────────────────────────

SRCS   := src/main.c src/lexer.c
OBJS   := $(SRCS:.c=.o)
TARGET := yamlget

# ── Lexer unit-test binary ────────────────────────────────────────────────────

TEST_LEXER_SRCS := tests/lexer/test_lexer.c src/lexer.c
TEST_LEXER_BIN  := tests/lexer/test_lexer

.PHONY: all clean test test-lexer test-lexer-build test-all install uninstall debug asan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Iinclude -c -o $@ $<

# ── Tests ─────────────────────────────────────────────────────────────────────

# `make test` runs only what is currently expected to pass (lexer tests).
# Integration tests (make test-all) require M3+ and will fail until then.
test: test-lexer

test-lexer: test-lexer-build
	@bash tests/lexer/run_lexer_tests.sh

test-lexer-build: $(TEST_LEXER_BIN)

$(TEST_LEXER_BIN): $(TEST_LEXER_SRCS)
	$(CC) $(CFLAGS) -Iinclude -o $@ $^

# Full integration tests — runs against the main binary; requires M3+.
test-all: all test-lexer
	@bash tests/run_tests.sh

# ── Install / uninstall ───────────────────────────────────────────────────────

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# ── Housekeeping ──────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe
	rm -f $(TEST_LEXER_BIN) $(TEST_LEXER_BIN).exe

# ── Developer targets ─────────────────────────────────────────────────────────

debug: CFLAGS += -g -DDEBUG -O0
debug: $(TARGET)

asan: CFLAGS += -g -fsanitize=address,undefined -O1
asan: $(TARGET)

asan-test: CFLAGS += -g -fsanitize=address,undefined -O1
asan-test: $(TEST_LEXER_BIN)
	@bash tests/lexer/run_lexer_tests.sh
