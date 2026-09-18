# ==============================================================================
# Portfolio-Grade Hospital/Residents Gale-Shapley Stable Matching Engine
# Compatible with GNU Make and MinGW32-Make across Linux, macOS, and Windows
# ==============================================================================

CC ?= gcc
CFLAGS_COMMON = -std=c99 -Wall -Wextra -pedantic
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -DNDEBUG
CFLAGS_DEBUG   = $(CFLAGS_COMMON) -g -O0 -DDEBUG

SRC_DIR = src
TEST_DIR = tests
BENCH_DIR = benchmarks

SRCS = $(SRC_DIR)/matching.c $(SRC_DIR)/verifier.c $(SRC_DIR)/parser.c
MAIN_SRC = $(SRC_DIR)/main.c
TEST_SRC = $(TEST_DIR)/test_runner.c
BENCH_SRC = $(BENCH_DIR)/benchmark.c

ifeq ($(OS),Windows_NT)
    TARGET = matching.exe
    TEST_TARGET = test_runner.exe
    BENCH_TARGET = benchmark.exe
    RM = del /Q /F
    SEP = \\
else
    TARGET = matching
    TEST_TARGET = test_runner
    BENCH_TARGET = benchmark
    RM = rm -f
    SEP = /
endif

.PHONY: all release debug test valgrind benchmark clean help

all: release

release:
	$(CC) $(CFLAGS_RELEASE) $(SRCS) $(MAIN_SRC) -o $(TARGET)

debug:
	$(CC) $(CFLAGS_DEBUG) $(SRCS) $(MAIN_SRC) -o $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(SRCS) $(TEST_SRC)
	$(CC) $(CFLAGS_RELEASE) $(SRCS) $(TEST_SRC) -o $(TEST_TARGET)

benchmark: $(BENCH_TARGET)
	./$(BENCH_TARGET)
	python $(BENCH_DIR)/plot_benchmark.py

$(BENCH_TARGET): $(SRCS) $(BENCH_SRC)
	$(CC) $(CFLAGS_RELEASE) $(SRCS) $(BENCH_SRC) -o $(BENCH_TARGET)

valgrind: $(TEST_TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./$(TEST_TARGET)

clean:
ifeq ($(OS),Windows_NT)
	-cmd /c del /Q *.exe *.o $(BENCH_DIR)\*.csv 2>NUL
else
	$(RM) $(TARGET) $(TEST_TARGET) $(BENCH_TARGET) *.o $(BENCH_DIR)/*.csv
endif

help:
	@echo "Available targets:"
	@echo "  make release    - Build optimized production binary"
	@echo "  make debug      - Build debug binary with symbols and asserts"
	@echo "  make test       - Build and execute unit test suite"
	@echo "  make benchmark  - Run scaling benchmarks and generate matplotlib charts"
	@echo "  make valgrind   - Run memory leak analysis under Valgrind"
	@echo "  make clean      - Remove build artifacts and temporary files"
