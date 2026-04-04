# 1. Force the shell to CMD
SHELL = cmd.exe

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -I./include -MMD -MP
LDFLAGS := -lsynchronization

TARGET  := shared-host.exe
DLL_TARGET := shared-host.dll
TEST_TARGET := test_main.exe
BENCH_TARGET := benchmark_main.exe

SRC_DIR := src
TEST_DIR := tests
OBJ_DIR := obj

CORE_SRC := src/shared_host_core.c src/shm_operations/shm_mapping.c
MAIN_SRC := src/main.c
TEST_SRC := tests/test_main.c
BENCH_SRC := tests/benchmark_main.c

CORE_OBJ := $(CORE_SRC:src/%.c=obj/%.o)
MAIN_OBJ := $(MAIN_SRC:src/%.c=obj/%.o)
TEST_OBJ := $(TEST_SRC:tests/%.c=obj/tests/%.o)
BENCH_OBJ := $(BENCH_SRC:tests/%.c=obj/tests/%.o)

DEP := $(CORE_OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d)

.PHONY: all clean dll test benchmark

all: $(TARGET) dll test benchmark

dll: $(DLL_TARGET)

test: $(TEST_TARGET)

benchmark: $(BENCH_TARGET)

$(TARGET): $(CORE_OBJ) $(MAIN_OBJ)
	@if not exist build mkdir build
	-@$(CC) $(CORE_OBJ) $(MAIN_OBJ) -o build/$@ $(LDFLAGS) 2>nul || echo Warning: Could not build $(TARGET) (Main probably missing)

$(DLL_TARGET): $(CORE_OBJ)
	@if not exist build mkdir build
	$(CC) -shared $(CORE_OBJ) -o build/$@ $(LDFLAGS)

$(TEST_TARGET): $(CORE_OBJ) $(TEST_OBJ)
	@if not exist build mkdir build
	$(CC) $(CORE_OBJ) $(TEST_OBJ) -o build/$@ $(LDFLAGS)

$(BENCH_TARGET): $(CORE_OBJ) $(BENCH_OBJ)
	@if not exist build mkdir build
	$(CC) $(CORE_OBJ) $(BENCH_OBJ) -o build/$@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist build\$(TARGET) del /q build\$(TARGET)
	@if exist build\$(DLL_TARGET) del /q build\$(DLL_TARGET)
	@if exist build\$(TEST_TARGET) del /q build\$(TEST_TARGET)
	@if exist build\$(BENCH_TARGET) del /q build\$(BENCH_TARGET)
