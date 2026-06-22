cmake_minimum_required(VERSION 3.16)
project(CommandGenerator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(CommandGenerator
        main.cpp
)

# nlohmann/json is header-only - just needs the include path
target_include_directories(CommandGenerator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Recommended compiler warnings
if(MSVC)
    target_compile_options(CommandGenerator PRIVATE /W4)
else()
    target_compile_options(CommandGenerator PRIVATE -Wall -Wextra -Wpedantic)
endif()