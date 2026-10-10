cmake_minimum_required(VERSION 3.16)
project(keysequenceedit_regression LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
add_executable(tst_keysequenceedit
    tst_keysequenceedit.cpp
    ../../src/gharqad/ui/utils/QtExtKeySequenceEdit.cpp)
target_include_directories(tst_keysequenceedit PRIVATE ../../src)
target_link_libraries(tst_keysequenceedit PRIVATE Qt6::Widgets Qt6::Test)
enable_testing()
add_test(NAME keysequenceedit COMMAND tst_keysequenceedit)
set_tests_properties(keysequenceedit PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
