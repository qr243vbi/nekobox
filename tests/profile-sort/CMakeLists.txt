cmake_minimum_required(VERSION 3.16)
project(profile_sort_regression LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
if(NOT DEFINED SORT_SOURCE_DIR)
    message(FATAL_ERROR "Run run_cpp_test.py to extract production source first")
endif()
add_executable(profile_sort_regression
    profile_sort_regression.cpp native_fixture.h
    "${SORT_SOURCE_DIR}/production_classes.h"
    "${SORT_SOURCE_DIR}/production_source.cpp")
target_include_directories(profile_sort_regression PRIVATE "${SORT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(profile_sort_regression PRIVATE Qt6::Core Qt6::Widgets Qt6::Test)
enable_testing()
add_test(NAME profile_sort_regression COMMAND profile_sort_regression)
set_tests_properties(profile_sort_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
