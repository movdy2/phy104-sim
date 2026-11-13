cmake_minimum_required(VERSION 3.21)
project(PHY104OpticalAssistant VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PHY104_BUILD_GUI   "Build the Qt6 desktop application" ON)
option(PHY104_BUILD_TESTS "Build unit tests"                  ON)

# Apply warning flags to each target explicitly so the production layers can use the
# stricter set without imposing new diagnostics on test fixtures.
function(phy104_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)   # /utf-8: Greek symbols in source files
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if("${ARGV1}" STREQUAL "STRICT")
            target_compile_options(${target} PRIVATE -Wshadow -Wold-style-cast -Wconversion)
        endif()
    endif()
endfunction()

# Business layer: pure C++, no Qt -> easy to unit test
add_subdirectory(src/core)

# Data layer + application (needs Qt 6)
if(PHY104_BUILD_GUI)
    add_subdirectory(src)
endif()

if(PHY104_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
