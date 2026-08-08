# ==============================================================================
# Shared-Host Makefile
# High-Performance Shared Memory IPC Library
# ==============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -O3 -I./include -MMD -MP

# Detect Operating System
ifeq ($(OS),Windows_NT)
    SHELL      := cmd.exe
    EXE        := .exe
    DLL        := .dll
    LDFLAGS    := -lsynchronization
    MKDIR      = @if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
    RMDIR      = @if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
    RM         = @if exist "$(subst /,\,$1)" del /q "$(subst /,\,$1)"
    RUN_CMD    = $(subst /,\,$1)
else
    EXE        :=
    DLL        := .so
    LDFLAGS    := -pthread
    MKDIR      = @mkdir -p "$1"
    RMDIR      = @rm -rf "$1"
    RM         = @rm -f "$1"
    RUN_CMD    = ./$1
endif

# Directories
SRC_DIR   := src
TEST_DIR  := tests
OBJ_DIR   := obj
BUILD_DIR := build

# Source Files
CORE_SRC  := $(SRC_DIR)/shared_host_core.c $(SRC_DIR)/shared_host_read.c $(SRC_DIR)/shared_host_write.c $(SRC_DIR)/shared_host_zc_write.c $(SRC_DIR)/shared_host_zc_send.c $(SRC_DIR)/shm_operations/shm_mapping.c
TEST_SRC  := $(TEST_DIR)/main.c $(TEST_DIR)/benchmark.c $(TEST_DIR)/history.c $(TEST_DIR)/comprehensive_tests.c

# Object Files
CORE_OBJ  := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRC))
TEST_OBJ  := $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/tests/%.o,$(TEST_SRC))

# Dependencies
DEP       := $(CORE_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

# Targets
DLL_TARGET    := $(BUILD_DIR)/shared-host$(DLL)
TEST_TARGET   := $(BUILD_DIR)/test_main$(EXE)
BENCH_TARGET  := $(BUILD_DIR)/benchmark_main$(EXE)
TB_TARGET     := $(BUILD_DIR)/testandbenchmark$(EXE)

.PHONY: all dll test benchmark testandbenchmark run run-test run-benchmark run-testandbenchmark clean help

all: dll test benchmark testandbenchmark

dll: $(DLL_TARGET)

test: $(TEST_TARGET)

benchmark: $(BENCH_TARGET)

testandbenchmark: $(TB_TARGET)

$(DLL_TARGET): $(CORE_OBJ)
	$(call MKDIR,$(BUILD_DIR))
	$(CC) -shared $(CORE_OBJ) -o $@ $(LDFLAGS)

$(TEST_TARGET): $(CORE_OBJ) $(TEST_OBJ)
	$(call MKDIR,$(BUILD_DIR))
	$(CC) $(CORE_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS)

$(BENCH_TARGET): $(CORE_OBJ) $(TEST_OBJ)
	$(call MKDIR,$(BUILD_DIR))
	$(CC) $(CORE_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS)

$(TB_TARGET): $(CORE_OBJ) $(TEST_OBJ)
	$(call MKDIR,$(BUILD_DIR))
	$(CC) $(CORE_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(call MKDIR,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c
	$(call MKDIR,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

run: run-test

run-test: test
	$(call RUN_CMD,$(TEST_TARGET))

run-benchmark: benchmark
	$(call RUN_CMD,$(BENCH_TARGET))

run-testandbenchmark: testandbenchmark
	$(call MKDIR,benchmarks)
	$(call RUN_CMD,$(TB_TARGET))

clean:
	$(call RMDIR,$(OBJ_DIR))
	$(call RMDIR,$(BUILD_DIR))

help:
	@echo Shared-Host Build System
	@echo Targets:
	@echo   all                  Build DLL, test suite, and benchmark binaries
	@echo   dll                  Build the shared library (shared-host.dll)
	@echo   test                 Build the test executable (test_main.exe)
	@echo   benchmark            Build the benchmark executable (benchmark_main.exe)
	@echo   testandbenchmark     Build the dual-mode test+benchmark suite
	@echo   run-test             Build and run the test suite
	@echo   run-benchmark        Build and run the benchmark suite
	@echo   run-testandbenchmark Build and run dual-mode suite (FAST+SLOW with history)
	@echo   clean                Remove build artifacts and object files
