APP := avr-emulator
SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/$(APP)
TEST_TARGET := $(BUILD_DIR)/tests/phase1_tests

CC ?= cc
DEBUGGER ?= lldb

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS :=
LDLIBS :=

SOURCES := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all compile build test run debug clean help

all: build

ifneq ($(strip $(SOURCES)),)
compile: $(OBJECTS)

build: $(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): tests/test_phase1.c src/mcu/avr_mcu.c \
                src/instructions/avr_instruction.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: build
	$(TARGET)

debug: build
	$(DEBUGGER) $(TARGET)
else
compile build:
	@echo "No C source files found under $(SRC_DIR)."

run debug:
	@echo "No executable exists yet; add a source file with main() first."
	@false
endif

clean:
	rm -rf $(BUILD_DIR)

help:
	@printf '%s\n' \
	  'make compile  Compile source files into build objects.' \
	  'make build    Build $(TARGET).' \
	  'make test     Run mcu reset and arithmetic flag tests.' \
	  'make run      Build and run the emulator.' \
	  'make debug    Build and open the emulator in $(DEBUGGER).' \
	  'make clean    Remove generated build files.'
