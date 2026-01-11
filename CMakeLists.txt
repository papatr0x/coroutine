cmake_minimum_required(VERSION 3.50)
project(coroutine_prj LANGUAGES CXX)

add_executable(coroutine_prj main.cpp)

target_compile_features(coroutine_prj PRIVATE cxx_std_20)
set_target_properties(coroutine_prj PROPERTIES CXX_STANDARD_REQUIRED ON)

# On Mac you need to tell clang where the SDK is installed
if(APPLE)
    execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    target_compile_options(coroutine_prj PRIVATE -isysroot ${MACOS_SDK_PATH})
endif()

# Enable warnings
if(MSVC)
    target_compile_options(coroutine_prj PRIVATE /W4)
else()
    target_compile_options(coroutine_prj PRIVATE -Wall -Wextra -Wpedantic)
endif()