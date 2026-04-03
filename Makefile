# 1. Force the shell to CMD
SHELL = cmd.exe

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -I./include -MMD -MP
LDFLAGS := 

TARGET  := shared-host.exe
SRC_DIR := src
OBJ_DIR := obj

SRC := src/main.c src/shared_host_core.c src/shm_operations/shm_mapping.c
OBJ := $(SRC:src/%.c=obj/%.o)
DEP := $(OBJ:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o build/$@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(TARGET) del /q $(TARGET)