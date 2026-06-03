VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_HEADER := include/version.h
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "v$(VERSION_MAJOR).$(VERSION_MINOR)-unknown")
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

.PHONY: all clean install uninstall FORCE

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
	@rm -rf $(BUILD_DIR) $(TARGET)
