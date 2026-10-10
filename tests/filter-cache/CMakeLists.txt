cmake_minimum_required(VERSION 3.16)
project(column_filter_cache_regression LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core)
if(NOT DEFINED PROXY_SOURCE_DIR)
    message(FATAL_ERROR "Run run_cpp_test.py to extract production source first")
endif()
add_executable(filter_cache_regression
    filter_cache_regression.cpp
    "${PROXY_SOURCE_DIR}/column_filter_proxy.h"
    "${PROXY_SOURCE_DIR}/column_filter_proxy.cpp")
target_include_directories(filter_cache_regression PRIVATE "${PROXY_SOURCE_DIR}")
target_link_libraries(filter_cache_regression PRIVATE Qt6::Core)
enable_testing()
add_test(NAME column_filter_cache COMMAND filter_cache_regression)
