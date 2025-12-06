# Makefile for static-ip-fix
# Uses CMake as the build system

BUILD_DIR = build
BIN_DIR = bin

.PHONY: all clean release debug vs

# Default: build with CMake
all:
	@cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
	@cmake --build $(BUILD_DIR)
	@cmake -E make_directory $(BIN_DIR)
	@cmake -E copy $(BUILD_DIR)/bin/static-ip-fix.exe $(BIN_DIR)/
	@cmake -E copy $(BUILD_DIR)/compile_commands.json .

# Debug build
debug:
	@cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR)
	@cmake -E make_directory $(BIN_DIR)
	@cmake -E copy $(BUILD_DIR)/bin/static-ip-fix.exe $(BIN_DIR)/
	@cmake -E copy $(BUILD_DIR)/compile_commands.json .

# Explicit release build
release: all

# Clean build artifacts
clean:
	@cmake -E rm -rf $(BUILD_DIR) $(BIN_DIR)

# Visual Studio build
vs:
	@cmake -S . -B $(BUILD_DIR) -G "Visual Studio 17 2022"
	@cmake --build $(BUILD_DIR) --config Release
	@cmake -E make_directory $(BIN_DIR)
	@cmake -E copy $(BUILD_DIR)/bin/Release/static-ip-fix.exe $(BIN_DIR)/
