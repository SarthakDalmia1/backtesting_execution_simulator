cmake_minimum_required(VERSION 3.16)
project(backtesting_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags - optimized for low-latency
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -ffast-math -funroll-loops")

# Find packages
find_package(Threads REQUIRED)

# Option to build Python bindings
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_TESTS "Build tests" ON)

# Core library
add_library(backtesting_engine STATIC
    cpp/core/types.cpp
    cpp/core/utils.cpp
    cpp/core/memory_pool.cpp
    cpp/core/timestamp.cpp
    cpp/orderbook/order.cpp
    cpp/orderbook/orderbook.cpp
    cpp/orderbook/matching_engine.cpp
    cpp/events/event.cpp
    cpp/events/event_queue.cpp
    cpp/execution/execution_simulator.cpp
    cpp/execution/slippage_model.cpp
    cpp/execution/transaction_costs.cpp
    cpp/position/position_manager.cpp
    cpp/position/pnl_tracker.cpp
    cpp/strategy/strategy_base.cpp
    cpp/data/market_data_feed.cpp
    cpp/data/tick_reader.cpp
)

# Enable position independent code for shared library linking
set_target_properties(backtesting_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(backtesting_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
)

target_link_libraries(backtesting_engine PUBLIC
    Threads::Threads
)

# Python bindings
if(BUILD_PYTHON_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    find_package(pybind11 CONFIG)
    
    if(pybind11_FOUND)
        pybind11_add_module(pybacktest python/bindings/bindings.cpp)
        target_link_libraries(pybacktest PRIVATE backtesting_engine)
        target_include_directories(pybacktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
    else()
        message(WARNING "pybind11 not found, skipping Python bindings")
    endif()
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_executable(benchmarks cpp/benchmarks/benchmark_main.cpp)
    target_link_libraries(benchmarks PRIVATE backtesting_engine)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_runner
        tests/test_main.cpp
        tests/test_orderbook.cpp
        tests/test_matching_engine.cpp
        tests/test_event_queue.cpp
        tests/test_execution.cpp
        tests/test_position.cpp
        tests/test_strategies.cpp
    )
    target_link_libraries(test_runner PRIVATE backtesting_engine)
    add_test(NAME AllTests COMMAND test_runner)
endif()
