# Compiler and flags
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic \
		  -Wformat=2 -Wcast-align -Wconversion -Wdouble-promotion \
		  -Wfloat-equal -Wpointer-arith -Wshadow -Wuninitialized \
		  -Wunused -Wvla -Wwrite-strings -Wstrict-prototypes \
		  -Wmissing-prototypes -Wredundant-decls

# AddressSanitizer + UBSan flags (enabled for 'test' target)
SAN_FLAGS = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer

# Coverage flags (enabled for 'coverage' target, not combined with SAN_FLAGS)
COV_FLAGS = --coverage

# Test files
TEST_DIR = test
TEST_SRC = $(TEST_DIR)/test_zring.c
TEST_BIN = $(TEST_DIR)/test_zring

# Coverage output
COV_DIR  = coverage
COV_INFO = $(COV_DIR)/coverage.info
COV_HTML = $(COV_DIR)/html

.PHONY: all test coverage clean help

all: test

# Build and run with AddressSanitizer + UBSan (default developer target)
test: $(TEST_BIN)
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) zring.h
	@$(CC) $(CFLAGS) $(SAN_FLAGS) -I. -o $@ $<

# Build with coverage instrumentation, run tests, and generate lcov report.
# Depends on 'clean' to avoid stale .gcda files contaminating coverage data.
coverage: clean
	@$(CC) $(CFLAGS) $(COV_FLAGS) -I. -o $(TEST_BIN) $(TEST_SRC)
	@./$(TEST_BIN)
	@mkdir -p $(COV_DIR)
	@lcov --capture --directory . --output-file $(COV_INFO) --ignore-errors unused
	@lcov --remove $(COV_INFO) '/usr/*' 'test/*' --output-file $(COV_INFO) --ignore-errors unused
	@genhtml $(COV_INFO) --output-directory $(COV_HTML)
	@echo "Coverage report: $(COV_HTML)/index.html"

clean:
	@rm -f $(TEST_BIN)
	@rm -rf $(COV_DIR)
	@find . -name "*.gcno" -delete 2>/dev/null || true
	@find . -name "*.gcda" -delete 2>/dev/null || true
	@find . -name "*.gcov" -delete 2>/dev/null || true

help:
	@echo "Available targets:"
	@echo "  all      - Build and run tests with AddressSanitizer (default)"
	@echo "  test     - Build and run tests with AddressSanitizer + UBSan"
	@echo "  coverage - Build with coverage, run tests, generate HTML report"
	@echo "  clean    - Remove generated files and coverage data"
