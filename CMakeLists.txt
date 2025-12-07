cmake_minimum_required(VERSION 3.16)
project(static-ip-fix
    VERSION 1.0.0
    DESCRIPTION "Configure static IP and DNS-over-HTTPS on Windows"
    LANGUAGES C
)

# Windows-only project
if(NOT WIN32)
    message(FATAL_ERROR "This project only supports Windows")
endif()

# C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate compile_commands.json for editor/LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Auto-detect source files
file(GLOB SOURCES "src/*.c")
file(GLOB TUI_SOURCES "src/tui/*.c")
file(GLOB HEADERS "include/*.h")
file(GLOB TUI_HEADERS "include/tui/*.h")

# Combine sources
list(APPEND SOURCES ${TUI_SOURCES})
list(APPEND HEADERS ${TUI_HEADERS})

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Unicode support
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0600
)

# Compiler-specific flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /O2)
else()
    # MinGW/GCC
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O2 -municode)
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    iphlpapi
    advapi32
    ws2_32
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/tui
)

# ============================================================================
# TEST TARGET
# ============================================================================

enable_testing()

# Test executable
add_executable(test_utils
    tests/test_utils.c
    src/utils.c
)

target_include_directories(test_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

target_compile_definitions(test_utils PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0600
)

if(MSVC)
    target_compile_options(test_utils PRIVATE /W4)
else()
    target_compile_options(test_utils PRIVATE -Wall -Wextra)
endif()

add_test(NAME utils_tests COMMAND test_utils)

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# CPack for creating installers (optional)
set(CPACK_PACKAGE_NAME "static-ip-fix")
set(CPACK_PACKAGE_VENDOR "static-ip-fix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Configure static IP and DNS-over-HTTPS on Windows")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
include(CPack)
