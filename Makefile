# yamlget Makefile
# Targets: all, clean, test, install, uninstall

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin

# Source files (add new .c files here as they are created)
SRCS    := src/main.c
OBJS    := $(SRCS:.c=.o)
TARGET  := yamlget

# Test runner script
TEST_RUNNER := tests/run_tests.sh

.PHONY: all clean test install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Iinclude -c -o $@ $<

test: $(TARGET)
	@if [ -f "$(TEST_RUNNER)" ]; then \
		bash $(TEST_RUNNER); \
	else \
		echo "No test runner found at $(TEST_RUNNER)"; \
		exit 1; \
	fi

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

# Developer targets
debug: CFLAGS += -g -DDEBUG -O0
debug: $(TARGET)

asan: CFLAGS += -g -fsanitize=address,undefined -O1
asan: $(TARGET)
