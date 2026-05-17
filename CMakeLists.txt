cmake_minimum_required(VERSION 3.16)
project(tangbase VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

add_library(tangbase INTERFACE)
target_include_directories(tangbase INTERFACE src)

enable_testing()

add_executable(async_task_runner examples/async_task_runner.cpp)
target_link_libraries(async_task_runner PRIVATE tangbase)

# --- test executables ---

add_executable(test_result tests/test_result.cpp)
target_link_libraries(test_result PRIVATE tangbase)
add_test(NAME test_result COMMAND test_result)

add_executable(test_thread_pool tests/test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE tangbase)
add_test(NAME test_thread_pool COMMAND test_thread_pool)

add_executable(test_sync tests/test_sync.cpp)
target_link_libraries(test_sync PRIVATE tangbase)
add_test(NAME test_sync COMMAND test_sync)

add_executable(test_assert tests/test_assert.cpp)
target_link_libraries(test_assert PRIVATE tangbase)
add_test(NAME test_assert COMMAND test_assert)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE tangbase)
add_test(NAME test_config COMMAND test_config)

add_executable(test_util tests/test_util.cpp)
target_link_libraries(test_util PRIVATE tangbase)
add_test(NAME test_util COMMAND test_util)

add_executable(test_event_loop tests/test_event_loop.cpp)
target_link_libraries(test_event_loop PRIVATE tangbase)
add_test(NAME test_event_loop COMMAND test_event_loop)

add_executable(test_event_bus tests/test_event_bus.cpp)
target_link_libraries(test_event_bus PRIVATE tangbase)
add_test(NAME test_event_bus COMMAND test_event_bus)

add_executable(test_logger tests/test_logger.cpp)
target_link_libraries(test_logger PRIVATE tangbase)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_debug tests/test_debug.cpp)
target_link_libraries(test_debug PRIVATE tangbase)