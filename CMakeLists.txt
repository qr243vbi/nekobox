cmake_minimum_required(VERSION 3.10)

project(nekobox VERSION 6.0.0)
# Use this snippet *after* PROJECT(xxx):
if(UNIX)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "default prefix" FORCE)
    endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
endif()

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
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
#	endif ()
#endif ()

set(NYXARIA "src/nekobox")
set(GHARQAD "src/gharqad")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network LinguistTools Concurrent)

if (NKR_CROSS)
    set_property(TARGET Qt6::moc PROPERTY IMPORTED_LOCATION /usr/bin/moc)
    set_property(TARGET Qt6::uic PROPERTY IMPORTED_LOCATION /usr/bin/uic)
    set_property(TARGET Qt6::rcc PROPERTY IMPORTED_LOCATION /usr/bin/rcc)
    set_property(TARGET Qt6::lrelease PROPERTY IMPORTED_LOCATION /usr/bin/lrelease)
    set_property(TARGET Qt6::lupdate PROPERTY IMPORTED_LOCATION /usr/bin/lupdate)
endif ()

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
option(SKIP_LMDB "Do not use lmdb" OFF)
option(SKIP_JS_UPDATER "Skip JS update checker" OFF)
option(USE_HOTKEYS "Skip HotKeys" ON)
option(BUILD_GO_PARTS "Build go parts" ON)

include("cmake/print.cmake")
include("cmake/nkr.cmake")

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
)

# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}
        ${THRIFT_GENERATED_SOURCES}
        ${SECURITY_SOURCES}
        ${PROJECT_SOURCES_GLOB}
        ${GHARQAD}/main.cpp

        3rdparty/base64.cpp
        3rdparty/qrcodegen.cpp
        3rdparty/QtExtKeySequenceEdit.cpp
        3rdparty/QrDecoder.cpp

        3rdparty/qv2ray/v2/ui/LogHighlighter.cpp
        3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp
        3rdparty/qv2ray/v2/ui/widgets/common/QJsonModel.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.ui
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.cpp
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.hpp
        3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.cpp

        3rdparty/quirc/decode.c
        3rdparty/quirc/identify.c
        3rdparty/quirc/quirc.c
        3rdparty/quirc/version_db.c

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
        ${NYXARIA}/ui/utils/MapListModel.hpp

        ${GHARQAD}/sys/Process.cpp
        ${GHARQAD}/sys/AutoRun.cpp

        ${GHARQAD}/ui/mainwindow_rpc.cpp
        ${GHARQAD}/ui/mainwindow.cpp
        ${NYXARIA}/ui/mainwindow.h
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
if(SKIP_LMDB)
    nkr_add_compile_definitions(SKIP_LMDB=yes)
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

# Target Source Translations

file(GLOB TS_FILES "res/translations/*.ts")

foreach(TS_FILE ${TS_FILES})
    # Get the base name of the file without the extension
    get_filename_component(QM_FILE ${TS_FILE} NAME_WE)
    # Define the output path for .qm files
    set(QM_FILE_PATH "${CMAKE_BINARY_DIR}/${QM_FILE}.qm")
    # Add the generated .qm file to the list
    list(APPEND QM_FILES ${QM_FILE_PATH})
endforeach()

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
        Qt6::Widgets Qt6::Network Qt6::Concurrent
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

if (NOT SKIP_LMDB)
    pkg_check_modules(LMDB REQUIRED lmdb)
    target_include_directories(nekobox PRIVATE ${LMDB_INCLUDE_DIRS})
    target_link_libraries(nekobox PRIVATE ${LMDB_LIBRARIES})
    target_compile_options(nekobox PRIVATE ${LMDB_CFLAGS_OTHER})
endif()

#qt_finalize_executable(nekobox)
if (UNIX)

remove_rpath(nekobox)

set(FILE_URL "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json")
set(DEST_FILE "${CMAKE_SOURCE_DIR}/srslist.json")

if(NOT EXISTS "${DEST_FILE}")
    message(STATUS "srslist.json not found in source dir, downloading: ${DEST_FILE}")

    file(DOWNLOAD
        "${FILE_URL}"
        "${DEST_FILE}"
        SHOW_PROGRESS
        STATUS download_status
        LOG download_log
    )

    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(STATUS
            "Download failed: ${download_status}\n${download_log}"
        )
    endif()
else()
    message(STATUS "File already exists: ${DEST_FILE}")
endif()

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

# Configure the desktop file
configure_file(
    "${CMAKE_SOURCE_DIR}/script/start_sing_box.sh.in"
    "${CMAKE_BINARY_DIR}/start_sing_box.sh"
    @ONLY
)


install(
    FILES "${CMAKE_SOURCE_DIR}/res/public/icon.png"
    RENAME "nekobox.png"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
)

install(
    FILES "${CMAKE_BINARY_DIR}/nekobox.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(
    PROGRAMS "${CMAKE_BINARY_DIR}/start.sh"
    RENAME "nekobox"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    PROGRAMS "${CMAKE_BINARY_DIR}/start_sing_box.sh"
    RENAME "sing-box"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/global.ini")
    install(FILES ${CMAKE_SOURCE_DIR}/global.ini DESTINATION "${PROGRAMPREFIX}/public")
endif()

if(NOT SKIP_UPDATER AND NOT SKIP_JS_UPDATER)
    install(FILES ${CMAKE_SOURCE_DIR}/res/public/check_new_release.js DESTINATION "${PROGRAMPREFIX}/public")
endif()

install(FILES ${QM_FILES} DESTINATION "${PROGRAMPREFIX}/public")
install(TARGETS nekobox DESTINATION "${PROGRAMPREFIX}")
install(FILES 
    "${CMAKE_SOURCE_DIR}/srslist.json"
    "${CMAKE_SOURCE_DIR}/res/public/announcement_message.js"
    "${CMAKE_SOURCE_DIR}/res/public/check_routeprofiles.js"
    "${CMAKE_SOURCE_DIR}/res/languages.txt" DESTINATION "${PROGRAMPREFIX}/public")

install_if_exists(${CMAKE_SOURCE_DIR}/res/public/emoji.ttf "${PROGRAMPREFIX}/public")
install_if_exists(${CMAKE_SOURCE_DIR}/res/public/about.html "${PROGRAMPREFIX}/public")

endif()

