cmake_minimum_required(VERSION 3.10)

project(nekobox VERSION 6.0.0)
# Use this snippet *after* PROJECT(xxx):
if(UNIX)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "default prefix" FORCE)
    endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    include(GNUInstallDirs)
endif()

include(ExternalProject)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_SUPPRESS_REGENERATION ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0069 NEW) #find package policy
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW) #boost config policy
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
   set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()


#if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
#	if (NOT MSVC)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
#	endif ()
#endif ()

set(NYXARIA "src/nekobox")
set(GHARQAD "src/gharqad")


# Target Source Translations

file(GLOB TS_FILES "res/translations/*.ts")

foreach(TS_FILE ${TS_FILES})
    # Get the base name of the file without the extension
    get_filename_component(QM_FILE ${TS_FILE} NAME_WE)
    # Define the output path for .qm files
    set(QM_FILE_PATH "${CMAKE_BINARY_DIR}/${QM_FILE}.qm")
    # Add the generated .qm file to the list
    list(APPEND QM_FILES ${QM_FILE_PATH})
    # Add the generated .qm filename to the list
    list(APPEND QM_FILENAMES ${QM_FILE})
endforeach()

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Concurrent)

if (NKR_CROSS)
    set_property(TARGET Qt6::moc PROPERTY IMPORTED_LOCATION /usr/bin/moc)
    set_property(TARGET Qt6::uic PROPERTY IMPORTED_LOCATION /usr/bin/uic)
    set_property(TARGET Qt6::rcc PROPERTY IMPORTED_LOCATION /usr/bin/rcc)
    set_property(TARGET Qt6::lrelease PROPERTY IMPORTED_LOCATION /usr/bin/lrelease)
    set_property(TARGET Qt6::lupdate PROPERTY IMPORTED_LOCATION /usr/bin/lupdate)
endif ()

include("cmake/print.cmake")
include("cmake/nkr.cmake")

#### Platform Variables ####
if (WIN32)
    include("cmake/windows/windows.cmake")
else ()
    include("cmake/unix/unix.cmake")
endif ()

#### default prefix path ####

message("[CMAKE_PREFIX_PATH] ${CMAKE_PREFIX_PATH}")

# for some cross toolchain
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
message("[CMAKE_FIND_ROOT_PATH] ${CMAKE_FIND_ROOT_PATH}")

#### NKR ####
option(SKIP_UPDATER "Always hide update button" ON)
if(UNIX)
set(MIGRATE_LMDB_DEFAULT OFF)
else()
set(MIGRATE_LMDB_DEFAULT ON)
endif()
option(MIGRATE_LMDB "Do not migrate from lmdb" ${MIGRATE_LMDB_DEFAULT})
option(SKIP_ROCKSDB "Do not use rocksdb" OFF)
option(SKIP_JS_UPDATER "Skip JS update checker" OFF)
option(USE_HOTKEYS "Skip HotKeys" ON)
option(BUILD_GO_PARTS "Build go parts" ON)
option(CPR_USE_SYSTEM_CURL "Use system cpr" ON)
option(CPR_ENABLE_INSTALL "Use cpr as bundled dependency" OFF)
option(ENABLE_PROFILE_FILTER "Enable profile filtering on subscription update" ON)

find_package(Threads)

if (NKR_PACKAGE)
    nkr_add_compile_definitions(NKR_CPP_USE_APPDATA)
endif ()

set(BUILD_SHARED_LIBS OFF)

if (USE_HOTKEYS)
  set(QHOTKEY_INSTALL OFF CACHE BOOL "Disable install QHotkey library")
  set(QHOTKEY_EXAMPLES OFF CACHE BOOL "Enable compiling examples")
  add_subdirectory("3rdparty/QHotkey")
  nkr_add_compile_definitions(USE_HOTKEYS)
  list(APPEND NKR_EXTERNAL_TARGETS qhotkey)
endif()


find_program(THRIFT_COMPILER thrift REQUIRED)

if(EXISTS "${THRIFT_COMPILER}/tools/thrift/thrift.exe" AND NOT IS_DIRECTORY "${THRIFT_COMPILER}/tools/thrift/thrift.exe")
     set(THRIFT_COMPILER "${THRIFT_COMPILER}/tools/thrift/thrift.exe")
     set(ENV{THRIFT_COMPILER} "${THRIFT_COMPILER}")
endif()

message(STATUS "Found Thrift " ${THRIFT_COMPILER})

set(THRIFT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/core/server/gen/libcore.thrift")
set(THRIFT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen-cpp")
file(MAKE_DIRECTORY "${THRIFT_GEN_DIR}")

execute_process(
#    OUTPUT "${THRIFT_GEN_DIR}/LibcoreService.h" "${THRIFT_GEN_DIR}/LibcoreService.cpp"
    COMMAND ${THRIFT_COMPILER} --gen cpp --out "${THRIFT_GEN_DIR}" "${THRIFT_FILE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Add a custom target to ensure the code generation runs
add_custom_target(generate_thrift_code ALL DEPENDS "${THRIFT_GEN_DIR}/LibcoreService.h")

# Include the generated files in your project
include_directories("${THRIFT_GEN_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THRIFT_GENERATED_SOURCES
    "${GHARQAD}/libcore.cpp"
)

# Check if the directory exists
if(EXISTS "${CMAKE_SOURCE_DIR}/core/server/vendor")
    message(STATUS "Directory core/server/vendor exists.")
else()
    message(STATUS "Directory core/server/vendor does not exist.")
endif()

file(GLOB_RECURSE PROJECT_SOURCES_GLOB
        ${GHARQAD}/configs/proxy/*.cpp
        ${NYXARIA}/configs/proxy/*.hpp
        ${NYXARIA}/configs/proxy/*.h

        ${NYXARIA}/dataStore/*.hpp
        ${NYXARIA}/dataStore/*.h
        ${GHARQAD}/dataStore/*.cpp

        ${NYXARIA}/global/*.hpp
        ${NYXARIA}/global/*.h
        ${GHARQAD}/global/*.cpp

        ${NYXARIA}/ui/info/*.ui
        ${NYXARIA}/ui/info/*.h
        ${NYXARIA}/ui/info/*.hpp

        ${NYXARIA}/ui/profile/*.h
        ${NYXARIA}/ui/profile/*.hpp
        ${NYXARIA}/ui/profile/*.ui
        ${GHARQAD}/ui/profile/*.cpp

        ${GHARQAD}/ui/setting/*.cpp
        ${NYXARIA}/ui/setting/*.h
        ${NYXARIA}/ui/setting/*.hpp
        ${NYXARIA}/ui/setting/*.ui

        ${GHARQAD}/ui/group/*.cpp
        ${NYXARIA}/ui/group/*.h
        ${NYXARIA}/ui/group/*.hpp
        ${NYXARIA}/ui/group/*.ui

        ${NYXARIA}/ui/utils/*.hpp
        ${NYXARIA}/ui/utils/*.h
        ${GHARQAD}/ui/utils/*.cpp
)

# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}
        ${THRIFT_GENERATED_SOURCES}
        ${SECURITY_SOURCES}
        ${PROJECT_SOURCES_GLOB}
        ${GHARQAD}/main.cpp

        3rdparty/qrcodegen/qrcodegen.cpp

        3rdparty/qv2ray/v2/ui/LogHighlighter.cpp
        3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp
        3rdparty/qv2ray/v2/ui/widgets/common/QJsonModel.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.ui
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.cpp
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.hpp
        3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.cpp

        3rdparty/quirc/lib/decode.c
        3rdparty/quirc/lib/identify.c
        3rdparty/quirc/lib/quirc.c
        3rdparty/quirc/lib/version_db.c

        ${GHARQAD}/api/RPC.cpp
        ${GHARQAD}/stats/traffic/TrafficLooper.cpp

        ${GHARQAD}/configs/ConfigBuilder.cpp


        ${NYXARIA}/configs/sub/GroupUpdater.hpp
        ${NYXARIA}/configs/ConfigBuilder.hpp
        ${NYXARIA}/api/RPC.h

        ${NYXARIA}/stats/traffic/TrafficLooper.hpp

        ${GHARQAD}/configs/sub/GroupUpdater.cpp
        ${NYXARIA}/sys/AutoRun.hpp
        ${NYXARIA}/sys/Process.hpp
        ${NYXARIA}/sys/Settings.h

        ${GHARQAD}/sys/Process.cpp
        ${GHARQAD}/sys/AutoRun.cpp

        ${GHARQAD}/ui/mainwindow_rpc.cpp
        ${GHARQAD}/ui/mainwindow.cpp
        ${GHARQAD}/ui/mainwindow_table.cpp
        ${NYXARIA}/ui/mainwindow.h
        ${NYXARIA}/ui/mainwindow_table.h
        ${NYXARIA}/ui/mainwindow.ui

        res/nekobox.qrc
        ${QV2RAY_RC}
        res/darkstyle.qrc
        res/qss/feiyangqingyun/qss.qrc

        ${GHARQAD}/sys/Settings.cpp
        ${GHARQAD}/sys/AutoRun.cpp
        ${GHARQAD}/sys/Process.cpp
        ${NYXARIA}/ui/mainwindow_interface.h
        ${NYXARIA}/stats/connections/connectionLister.hpp
        ${GHARQAD}/stats/connectionLister/connectionLister.cpp
        ${GHARQAD}/configs/proxy/Json2Bean.cpp

        ${GHARQAD}/stats/autotester/ProxyAutoTester.cpp
        ${NYXARIA}/stats/autotester/ProxyAutoTester.hpp

)

# Qt exe
qt_add_executable(nekobox
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

# Target
set_property(TARGET nekobox PROPERTY AUTOUIC ON)
set_property(TARGET nekobox PROPERTY AUTOMOC ON)
set_property(TARGET nekobox PROPERTY AUTORCC ON)

if(SKIP_UPDATER)
    nkr_add_compile_definitions(SKIP_UPDATE_BUTTON=yes)
endif()
if(SKIP_ROCKSDB)
    nkr_add_compile_definitions(SKIP_ROCKSDB=yes)
else()
    nkr_add_compile_definitions(LMDB_ENABLE_MUTEX=yes)
    nkr_add_compile_definitions(LMDB_ENABLE_MAPSIZE_METRICS=yes)
endif()
if(SKIP_JS_UPDATER)
    nkr_add_compile_definitions(SKIP_JS_UPDATER=yes)
else()
    find_package(Qt6 REQUIRED COMPONENTS Qml)
    target_link_libraries(nekobox PRIVATE Qt6::Qml)
    target_sources(nekobox
            PRIVATE
            ${GHARQAD}/js/js_updater.cpp
            ${NYXARIA}/js/js_updater.h
            ${NYXARIA}/js/blocking_queue.h
            ${NYXARIA}/js/js_updater.h
            ${NYXARIA}/js/message_queue.h
            ${NYXARIA}/js/version.h

            res/nekobox_updater.qrc
    )
endif()


target_include_directories(nekobox PRIVATE
    ${Qt6Protobuf_PRIVATE_INCLUDE_DIRS}
)

set_target_properties(nekobox PROPERTIES
        WIN32_EXECUTABLE TRUE
)


set(LUPDATE_OPTIONS
        -locations none -no-obsolete
)
qt_add_lupdate(nekobox TS_FILES ${TS_FILES} OPTIONS ${LUPDATE_OPTIONS})
qt_add_lrelease(nekobox TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)

# configure_file(res/translations/translations.qrc ${CMAKE_BINARY_DIR} COPYONLY)
# target_sources(nekobox PRIVATE ${CMAKE_BINARY_DIR}/translations.qrc)

if (MSVC)
    find_package(Thrift REQUIRED)
    set(THRIFT_LIBRARIES thrift::thrift)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(THRIFT thrift REQUIRED)
endif()

target_include_directories(nekobox PRIVATE ${THRIFT_INCLUDE_DIRS})
target_link_libraries(nekobox PRIVATE ${THRIFT_LIBRARIES})


# Try system-installed CPR first
find_package(cpr CONFIG QUIET)

if(cpr_FOUND)
    message(STATUS "Using system CPR")
else()
    message(STATUS "System CPR not found, using bundled version")
    add_subdirectory("${CMAKE_SOURCE_DIR}/3rdparty/cpr")
endif()

target_link_libraries(nekobox PRIVATE cpr::cpr)

#if (MSVC)
#    include(FindBoost)
#    set(Boost_USE_STATIC_LIBS ON)
#    set(Boost_USE_MULTITHREADED ON)
#    set(Boost_USE_STATIC_RUNTIME ON)
#endif()

find_package(Boost REQUIRED COMPONENTS filesystem)
target_include_directories(nekobox PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(nekobox PRIVATE Boost::filesystem)
if (UNIX)
find_library(ACL_LIB acl)

if(NOT ACL_LIB)
    message(FATAL_ERROR "libacl not found. Install libacl-devel.")
endif()

target_link_libraries(nekobox PRIVATE ${ACL_LIB})
endif()

# Target Link

target_link_libraries(nekobox PRIVATE
        Qt6::Widgets Qt6::Concurrent
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

#find_package(leveldb REQUIRED)
#target_link_libraries(nekobox PRIVATE leveldb::leveldb)

if (MSVC)
    target_compile_options(nekobox PRIVATE "/std:c++20")       # add c++20 feature
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set_target_properties(nekobox PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

if (BUILD_GO_PARTS)
    find_program(GO_EXECUTABLE go)


# Check if the vendor directory exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/core/server/vendor")
        message(STATUS "Directory core/server/vendor exists.")
        set(GO_MOD_TIDY_FLAG OFF CACHE STRING "Run go mod tidy before build")
    else()
        message(STATUS "Directory core/server/vendor does not exist.")
        set(GO_MOD_TIDY_FLAG ON CACHE STRING "Run go mod tidy before build")
    endif()

    set(GO_SOURCE_DIR ${CMAKE_SOURCE_DIR}/core)

    if(EXISTS ${GO_SOURCE_DIR}/CMakeLists.txt AND GO_EXECUTABLE)
        add_subdirectory(${GO_SOURCE_DIR})
    else()
        message(STATUS "Go server not found, skipping")
    endif()
endif()

if (NOT SKIP_ROCKSDB)
    if (UNIX)
        pkg_check_modules(ROCKSDB rocksdb)
        target_include_directories(nekobox PRIVATE ${ROCKSDB_INCLUDE_DIRS})
        target_link_libraries(nekobox PRIVATE ${ROCKSDB_LIBRARIES})
        target_compile_options(nekobox PRIVATE ${ROCKSDB_CFLAGS_OTHER})
    else()
        find_package(RocksDB CONFIG REQUIRED)
        target_link_libraries(example PRIVATE RocksDB::rocksdb)
    endif()
endif()

if (MIGRATE_LMDB)
    if (MSVC)
        find_package(unofficial-lmdb CONFIG REQUIRED)
        target_link_libraries(nekobox PRIVATE unofficial::lmdb::lmdb)
    else()
        pkg_check_modules(LMDB lmdb)
        if(LMDB_FOUND)
            message(STATUS "LMDB found via pkg-config")
            target_include_directories(nekobox PRIVATE ${LMDB_INCLUDE_DIRS})
            target_link_libraries(nekobox PRIVATE ${LMDB_LIBRARIES})
            target_compile_options(nekobox PRIVATE ${LMDB_CFLAGS_OTHER})
        else()
            message(STATUS "LMDB not found via pkg-config, using manual search")
            find_path(LMDB_INCLUDE_DIR lmdb.h)
            find_library(LMDB_LIBRARY lmdb)
            if(NOT LMDB_INCLUDE_DIR OR NOT LMDB_LIBRARY)
                message(FATAL_ERROR "LMDB not found")
            endif()
            target_include_directories(nekobox PRIVATE ${LMDB_INCLUDE_DIR})
            target_link_libraries(nekobox PRIVATE ${LMDB_LIBRARY})
        endif()
    endif()
    nkr_add_compile_definitions(MIGRATE_LMDB=yes)
endif()

find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(nekobox
    PRIVATE
    yaml-cpp::yaml-cpp
)

#qt_finalize_executable(nekobox)
if (UNIX)
download_srslist()

remove_rpath(nekobox)

set(PROGRAMPREFIX "${CMAKE_INSTALL_LIBEXECDIR}/Iblis" CACHE STRING "Installation Directory")

get_filename_component(START "${CMAKE_INSTALL_BINDIR}/nekobox" REALPATH BASE_DIR "${CMAKE_INSTALL_PREFIX}")
get_filename_component(MAIN "${PROGRAMPREFIX}/nekobox" REALPATH BASE_DIR "${CMAKE_INSTALL_PREFIX}")
get_filename_component(ICON "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps/nekobox.png" REALPATH BASE_DIR "${CMAKE_INSTALL_PREFIX}")

# Configure the desktop file
configure_file(
    "${CMAKE_SOURCE_DIR}/script/nekobox.desktop.in"
    "${CMAKE_BINARY_DIR}/nekobox.desktop"
    @ONLY
)

# Configure the desktop file
configure_file(
    "${CMAKE_SOURCE_DIR}/script/start.sh.in"
    "${CMAKE_BINARY_DIR}/start.sh"
    @ONLY
)

install(
    FILES "${CMAKE_SOURCE_DIR}/res/public/icon.png"
    RENAME "nekobox.png"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
    COMPONENT gui
)

install(
    FILES "${CMAKE_BINARY_DIR}/nekobox.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT gui
)

install(
    PROGRAMS "${CMAKE_BINARY_DIR}/start.sh"
    RENAME "nekobox"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT gui
)

if(EXISTS "${CMAKE_SOURCE_DIR}/global.ini")
    install(FILES ${CMAKE_SOURCE_DIR}/global.ini DESTINATION "${PROGRAMPREFIX}/public" COMPONENT gui)
endif()

if(NOT SKIP_UPDATER AND NOT SKIP_JS_UPDATER)
    install(FILES ${CMAKE_SOURCE_DIR}/res/public/check_new_release.js DESTINATION "${PROGRAMPREFIX}/public" COMPONENT gui)
endif()

install(FILES ${QM_FILES} DESTINATION "${PROGRAMPREFIX}/public" COMPONENT gui)
install(TARGETS nekobox DESTINATION "${PROGRAMPREFIX}" COMPONENT gui)
install(FILES 
    "${CMAKE_SOURCE_DIR}/srslist.json"
    "${CMAKE_SOURCE_DIR}/res/public/announcement_message.js"
    "${CMAKE_SOURCE_DIR}/res/public/check_routeprofiles.js"
    "${CMAKE_SOURCE_DIR}/res/languages.txt" DESTINATION "${PROGRAMPREFIX}/public"
    COMPONENT gui)

install_if_exists(${CMAKE_SOURCE_DIR}/res/public/emoji.ttf "${PROGRAMPREFIX}/public")
install_if_exists(${CMAKE_SOURCE_DIR}/res/public/about.html "${PROGRAMPREFIX}/public")

endif()

