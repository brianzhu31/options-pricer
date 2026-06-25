cmake_minimum_required(VERSION 3.20)

project(options_pricer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(options_pricer_core
    src/black_scholes.cpp
    src/monte_carlo.cpp
    src/payoff.cpp
)

target_include_directories(options_pricer_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(options_pricer_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(options_pricer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(options_pricer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(options-pricer src/main.cpp)
target_link_libraries(options-pricer PRIVATE options_pricer_core)

enable_testing()

add_executable(options_pricer_tests tests/pricer_tests.cpp)
target_link_libraries(options_pricer_tests PRIVATE options_pricer_core)

add_test(NAME options_pricer_tests COMMAND options_pricer_tests)

