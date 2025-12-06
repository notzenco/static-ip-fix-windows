# Makefile for static-ip-fix
# Uses CMake as the build system

BUILD_DIR = build
TARGET = $(BUILD_DIR)/bin/static-ip-fix.exe

.PHONY: all clean release debug

# Default: build with CMake
all:
	@cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
	@cmake --build $(BUILD_DIR)

# Debug build
debug:
	@cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR)

# Explicit release build
release: all

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR) 2>/dev/null || rmdir /S /Q $(BUILD_DIR) 2>nul || true

# Visual Studio build
vs:
	@cmake -S . -B $(BUILD_DIR) -G "Visual Studio 17 2022"
	@cmake --build $(BUILD_DIR) --config Release
