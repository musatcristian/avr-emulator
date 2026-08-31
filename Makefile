APP := avr-emulator
SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/$(APP)
TEST_TARGET := $(BUILD_DIR)/tests/test_runner
TUI_TARGET := $(BUILD_DIR)/avr-tui
GUI_TARGET := $(BUILD_DIR)/avr-gui

CC ?= cc
DEBUGGER ?= lldb

CPPFLAGS := -Iinclude
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -g
LDFLAGS :=
LDLIBS :=
TUI_LDLIBS := -lncurses

# SDL3 + SDL3_ttf are optional; prefer pkg-config, fall back to plain -l flags.
GUI_PKG_CONFIG_OK := $(shell pkg-config --exists sdl3 sdl3-ttf 2>/dev/null && echo yes)
ifeq ($(GUI_PKG_CONFIG_OK),yes)
GUI_CFLAGS := $(shell pkg-config --cflags sdl3 sdl3-ttf)
GUI_LDLIBS := $(shell pkg-config --libs sdl3 sdl3-ttf)
else
GUI_CFLAGS :=
GUI_LDLIBS := -lSDL3 -lSDL3_ttf
endif

# Each frontend owns exactly one src/ui/*.c file so their builds never pull in
# another frontend's main().
TUI_SOURCE := $(SRC_DIR)/ui/avr_tui.c
GUI_SOURCE := $(SRC_DIR)/ui/avr_gui.c
UI_SOURCES := $(shell find $(SRC_DIR)/ui -type f -name '*.c' 2>/dev/null)
SOURCES := $(filter-out $(UI_SOURCES),$(shell find $(SRC_DIR) -type f -name '*.c'))
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TEST_SOURCES := $(shell find tests -type f -name '*.c') \
               $(filter-out $(SRC_DIR)/main/%.c,$(SOURCES))
TUI_SOURCES := $(filter-out $(SRC_DIR)/main/%.c,$(SOURCES)) $(TUI_SOURCE)
GUI_SOURCES := $(filter-out $(SRC_DIR)/main/%.c,$(SOURCES)) $(GUI_SOURCE)

.PHONY: all compile build test tui gui run debug clean help

all: build

ifneq ($(strip $(SOURCES)),)
compile: $(OBJECTS)

build: $(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TUI_TARGET): $(TUI_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(TUI_LDLIBS) -o $@

$(GUI_TARGET): $(GUI_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) $^ $(LDFLAGS) $(GUI_LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: build
	$(TARGET)

debug: build
	$(DEBUGGER) $(TARGET)

tui: $(TUI_TARGET)
	$(TUI_TARGET)

gui: $(GUI_TARGET)
	$(GUI_TARGET)
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
	  'make test     Run the Phase 1 through Phase 10 test suite.' \
	  'make tui      Build and run the optional ncurses frontend.' \
	  'make gui      Build and run the optional SDL3 frontend (requires SDL3 + SDL3_ttf).' \
	  'make run      Build and run the emulator.' \
	  'make debug    Build and open the emulator in $(DEBUGGER).' \
	  'make clean    Remove generated build files.'
