# Makefile for terminal K-map solver
# Optimized for minimal build time and small executable size

CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -pedantic
LDFLAGS = -shared -fPIC
TEST_FLAGS = -g -DDEBUG -fsanitize=address

# Directories
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Source files
CORE_SRC = $(SRC_DIR)/kmap_core.c
HEADER = $(SRC_DIR)/kmap_core.h
PYTHON_INTERFACE = $(SRC_DIR)/kmapper.py

# Test files
TEST_SRC = $(TEST_DIR)/test_kmap_core.c
TEST_RUNNER = $(TEST_DIR)/run_tests.c

# Targets
.PHONY: all clean test install performance debug help

all: kmapper

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build shared library (production)
$(BUILD_DIR)/kmap_core.so: $(CORE_SRC) $(HEADER) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CORE_SRC)
	@echo "Built production library: $@"

# Build debug version for testing
$(BUILD_DIR)/libkmap_core_debug.so: $(CORE_SRC) $(HEADER) | $(BUILD_DIR)
	$(CC) $(TEST_FLAGS) $(LDFLAGS) -o $@ $(CORE_SRC)
	@echo "Built debug library: $@"

# Create executable Python script
kmapper: $(BUILD_DIR)/kmap_core.so $(PYTHON_INTERFACE)
	cp $(PYTHON_INTERFACE) kmapper
	chmod +x kmapper
	@echo "Created executable: kmapper"

# Build and run unit tests
test: $(BUILD_DIR)/libkmap_core_debug.so $(TEST_SRC)
	$(CC) $(TEST_FLAGS) -o $(BUILD_DIR)/test_runner $(TEST_SRC) -L$(BUILD_DIR) -lkmap_core_debug
	LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/test_runner
	@echo "All tests completed"

# Performance benchmarking
performance: kmapper
	@echo "Running performance tests..."
	@./performance_test.sh

# Debug build for development
debug: $(BUILD_DIR)/libkmap_core_debug.so
	@echo "Debug build complete"

# Install to system
install: kmapper
	sudo cp kmapper /usr/local/bin/
	sudo cp $(BUILD_DIR)/kmap_core.so /usr/local/lib/
	@echo "Installed to system"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) kmapper
	@echo "Cleaned build artifacts"

# Show available targets
help:
	@echo "Available targets:"
	@echo "  all        - Build production version"
	@echo "  test       - Build and run unit tests"
	@echo "  debug      - Build debug version"
	@echo "  performance- Run performance benchmarks"
	@echo "  install    - Install to system"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this help"

# Size analysis
size: kmapper
	@echo "Executable sizes:"
	@ls -lh kmapper $(BUILD_DIR)/kmap_core.so
	@echo "Line counts:"
	@wc -l $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(SRC_DIR)/*.py
