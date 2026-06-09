VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION_HEADER := include/version.h
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || \
	echo "v$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)-unknown")
BUILD_DATE := $(shell date +'%Y-%m-%d %H:%M:%S')

CC = gcc
CFLAGS = -std=c17 -Iinclude -Wall -O3
DEPFLAGS = -MMD -MP
CFLAGS += $(DEPFLAGS)

LIBS = -lncurses

SRC_DIR = src
BUILD_DIR = build
TARGET  = orp

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

SRC = $(wildcard ${SRC_DIR}/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJ:.o=.d)

TEST_DIR   = tests
TEST_BUILD = tests/build
TEST_STUBS = tests/stubs

TEST_SHARED_SRCS = $(SRC_DIR)/gap.c $(SRC_DIR)/buffer.c \
                   $(SRC_DIR)/config.c $(SRC_DIR)/fileio.c
TEST_SHARED_OBJS = $(TEST_SHARED_SRCS:$(SRC_DIR)/%.c=$(TEST_BUILD)/%.o)
TEST_INPUT_OBJ   = $(TEST_BUILD)/input_stub.o

TEST_CFLAGS = -std=c17 -Wall -Iinclude -I$(TEST_STUBS)

TEST_BINS = $(TEST_BUILD)/test_gap    \
            $(TEST_BUILD)/test_buffer \
            $(TEST_BUILD)/test_config \
            $(TEST_BUILD)/test_fileio \
            $(TEST_BUILD)/test_input

.PHONY: all clean install uninstall debug test FORCE

all: $(VERSION_HEADER) $(TARGET)

$(TARGET): $(OBJ)
	@echo "Linking $@"
	@$(CC) $(OBJ) -o $(TARGET) $(LIBS)

$(VERSION_HEADER): FORCE
	@mkdir -p include
	@echo "Generating $@"
	@echo "#ifndef VERSION_H" > $@
	@echo "#define VERSION_H" >> $@
	@echo "#define ORPHEUS_VERSION \"$(GIT_VERSION)\"" >> $@
	@echo "#define BUILD_DATE \"$(BUILD_DATE)\"" >> $@
	@echo "#endif" >> $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(VERSION_HEADER) | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

debug: CFLAGS += -DDEBUG -g
debug: all

install: all
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 755 $(TARGET) $(BINDIR)

uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)

-include $(DEPS)

clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR) $(TARGET) $(TEST_BUILD)

$(TEST_BUILD):
	@mkdir -p $(TEST_BUILD)

$(TEST_BUILD)/%.o: $(SRC_DIR)/%.c $(VERSION_HEADER) | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_INPUT_OBJ): $(SRC_DIR)/input.c $(VERSION_HEADER) | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_BUILD)/test_gap: $(TEST_DIR)/test_gap.c $(TEST_BUILD)/gap.o | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD)/test_buffer: $(TEST_DIR)/test_buffer.c \
    $(TEST_SHARED_OBJS) | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD)/test_config: $(TEST_DIR)/test_config.c \
    $(TEST_BUILD)/config.o | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD)/test_fileio: $(TEST_DIR)/test_fileio.c \
    $(TEST_SHARED_OBJS) | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD)/test_input: $(TEST_DIR)/test_input.c \
    $(TEST_SHARED_OBJS) $(TEST_INPUT_OBJ) | $(TEST_BUILD)
	@$(CC) $(TEST_CFLAGS) -o $@ $^

test: $(VERSION_HEADER) $(TEST_BINS)
	@echo ""
	@echo "========================================"
	@echo "  Orpheus Test Suite"
	@echo "========================================"
	@failed=0; \
	for bin in $(TEST_BINS); do \
	    echo ""; \
	    echo "--- $$bin ---"; \
	    $$bin; \
	    if [ $$? -ne 0 ]; then failed=$$((failed + 1)); fi; \
	done; \
	echo ""; \
	if [ $$failed -eq 0 ]; then \
	    echo "All suites passed."; \
	else \
	    echo "$$failed suite(s) had failures."; \
	    exit 1; \
	fi