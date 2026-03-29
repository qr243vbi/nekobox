cmake_minimum_required(VERSION 3.10)

project(nekobox VERSION 6.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0069 NEW) #find package policy
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW) #boost config policy
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

if (UNIX)
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
   set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()
endif()

#if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
#	if (NOT MSVC)
#		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
#	endif ()
#endif ()

set(NYXARIA "src/nekobox")
set(GHARQAD "src/gharqad")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network LinguistTools)

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
option(SKIP_UPDATE_BUTTON "Always hide update button" OFF)
option(SKIP_JS_UPDATER "Skip JS update checker" OFF)
option(USE_HOTKEYS "Skip JS update checker" ON)

include("cmake/print.cmake")
include("cmake/nkr.cmake")

find_package(Threads)

if (NKR_PACKAGE)
    nkr_add_compile_definitions(NKR_CPP_USE_APPDATA)
endif ()

set(BUILD_SHARED_LIBS OFF)

if (USE_HOTKEYS)
  set(QHOTKEY_INSTALL OFF CACHE BOOL "Disable install QHotkey library")
  set(QHOTKEY_EXAMPLES ON CACHE BOOL "Enable compiling examples")
  add_subdirectory("3rdparty/QHotkey")
  nkr_add_compile_definitions(USE_HOTKEYS)
  list(APPEND NKR_EXTERNAL_TARGETS qhotkey)
endif()

find_program(THRIFT_COMPILER thrift REQUIRED)
set(THRIFT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/core/server/gen/libcore.thrift")
set(THRIFT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen-cpp")
file(MAKE_DIRECTORY "${THRIFT_GEN_DIR}")
add_custom_command(
    OUTPUT "${THRIFT_GEN_DIR}/LibcoreService.h" "${THRIFT_GEN_DIR}/LibcoreService.cpp"
    COMMAND ${THRIFT_COMPILER} --gen cpp --out "${THRIFT_GEN_DIR}" "${THRIFT_FILE}"
    DEPENDS "${THRIFT_FILE}"
    COMMENT "Generating Thrift C++ code for Libcore"
)

# Add a custom target to ensure the code generation runs
add_custom_target(generate_thrift_code ALL DEPENDS "${THRIFT_GEN_DIR}/LibcoreService.h")

# Include the generated files in your project
include_directories("${THRIFT_GEN_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THRIFT_GENERATED_SOURCES
    "${GHARQAD}/libcore.cpp"
)


# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}
        ${THRIFT_GENERATED_SOURCES}
        ${SECURITY_SOURCES}
        ${GHARQAD}/main.cpp
        ${GHARQAD}/dataStore/Configs.cpp
        ${GHARQAD}/dataStore/ConfigItem.cpp
        ${GHARQAD}/dataStore/Utils.cpp
        ${GHARQAD}/global/GuiUtils.cpp
        ${GHARQAD}/global/HTTPRequestHelper.cpp
        ${GHARQAD}/global/DeviceDetailsHelper.cpp
        ${GHARQAD}/global/CountryHelper.cpp

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
        ${GHARQAD}/dataStore/Database.cpp
        ${GHARQAD}/stats/traffic/TrafficLooper.cpp
        ${GHARQAD}/dataStore/ProfileFilter.cpp
        ${GHARQAD}/configs/ConfigBuilder.cpp

        ${GHARQAD}/configs/proxy/AbstractBean.cpp
        ${GHARQAD}/configs/proxy/WireguardBean.cpp
        ${GHARQAD}/configs/proxy/Bean2CoreObj_box.cpp
        ${GHARQAD}/configs/proxy/Bean2Link.cpp
        ${GHARQAD}/configs/proxy/Link2Bean.cpp
        ${NYXARIA}/configs/proxy/AbstractBean.hpp
        ${NYXARIA}/configs/proxy/AnyTLSBean.hpp
        ${NYXARIA}/configs/proxy/ChainBean.hpp
        ${NYXARIA}/configs/proxy/CustomBean.hpp
        ${NYXARIA}/configs/proxy/ExtraCore.h
        ${NYXARIA}/configs/proxy/includes.h
        ${NYXARIA}/configs/proxy/TrustTunnelBean.hpp
        ${NYXARIA}/configs/proxy/JuicityBean.hpp
        ${NYXARIA}/configs/proxy/MieruBean.hpp
        ${NYXARIA}/configs/proxy/NaiveBean.hpp
        ${NYXARIA}/configs/proxy/TorBean.hpp
        ${NYXARIA}/configs/proxy/Preset.hpp
        ${NYXARIA}/configs/proxy/QUICBean.hpp
        ${NYXARIA}/configs/proxy/ShadowSocksBean.hpp
        ${NYXARIA}/configs/proxy/ShadowTLSBean.hpp
        ${NYXARIA}/configs/proxy/HttpBean.hpp
        ${NYXARIA}/configs/proxy/SocksBean.hpp
        ${NYXARIA}/configs/proxy/SSHBean.h
        ${NYXARIA}/configs/proxy/Tailscale.hpp
        ${NYXARIA}/configs/proxy/TrojanVLESSBean.hpp
        ${NYXARIA}/configs/proxy/V2RayStreamSettings.hpp
        ${NYXARIA}/configs/proxy/VMessBean.hpp
        ${NYXARIA}/configs/proxy/WireguardBean.h
        ${NYXARIA}/configs/sub/GroupUpdater.hpp
        ${NYXARIA}/configs/ConfigBuilder.hpp
        ${NYXARIA}/api/RPC.h
        ${NYXARIA}/dataStore/ConfigItem.hpp
        ${NYXARIA}/dataStore/Configs.hpp
        ${NYXARIA}/dataStore/Const.hpp
        ${NYXARIA}/dataStore/Database.hpp
        ${NYXARIA}/dataStore/DataStore.hpp
        ${NYXARIA}/dataStore/Group.hpp
        ${NYXARIA}/dataStore/ProfileFilter.hpp
        ${NYXARIA}/dataStore/ProxyEntity.hpp
        ${NYXARIA}/dataStore/ResourceEntity.hpp
        ${NYXARIA}/dataStore/RouteEntity.h
        ${NYXARIA}/dataStore/Utils.hpp

        ${NYXARIA}/global/CountryHelper.hpp
        ${NYXARIA}/global/DeviceDetailsHelper.hpp
        ${NYXARIA}/global/GuiUtils.hpp
        ${NYXARIA}/global/HTTPRequestHelper.hpp
        ${NYXARIA}/global/keyvaluerange.h

        ${NYXARIA}/stats/traffic/TrafficData.hpp
        ${NYXARIA}/stats/traffic/TrafficLooper.hpp

        ${GHARQAD}/configs/sub/GroupUpdater.cpp
        ${NYXARIA}/sys/AutoRun.hpp
        ${NYXARIA}/sys/Process.hpp
        ${NYXARIA}/sys/Settings.h
        ${NYXARIA}/ui/utils/MapListModel.hpp

        ${GHARQAD}/sys/Process.cpp
        ${GHARQAD}/sys/AutoRun.cpp

        ${NYXARIA}/ui/setting/ThemeManager.hpp
        ${GHARQAD}/ui/setting/ThemeManager.cpp
        ${GHARQAD}/ui/setting/Icon.cpp

        ${GHARQAD}/ui/mainwindow_rpc.cpp
        ${GHARQAD}/ui/mainwindow.cpp
        ${NYXARIA}/ui/mainwindow.h
        ${NYXARIA}/ui/mainwindow.ui
        ${NYXARIA}/ui/info/main.ui
        ${NYXARIA}/ui/info/info.h
        ${NYXARIA}/ui/profile/edit_shadowtls.ui
        ${NYXARIA}/ui/profile/dialog_edit_profile.h
        ${GHARQAD}/ui/profile/dialog_edit_profile.cpp
        ${NYXARIA}/ui/profile/dialog_edit_profile.ui
        ${NYXARIA}/ui/group/dialog_edit_group.h
        ${GHARQAD}/ui/group/dialog_edit_group.cpp
        ${NYXARIA}/ui/group/dialog_edit_group.ui
        ${NYXARIA}/ui/group/dialog_group_choose_proxy.ui

        ${NYXARIA}/ui/profile/edit_chain.h
        ${GHARQAD}/ui/profile/edit_chain.cpp
        ${NYXARIA}/ui/profile/edit_chain.ui
        ${NYXARIA}/ui/profile/edit_socks_http.h
        ${GHARQAD}/ui/profile/edit_socks_http.cpp
        ${NYXARIA}/ui/profile/edit_socks_http.ui
        ${NYXARIA}/ui/profile/edit_shadowsocks.h
        ${GHARQAD}/ui/profile/edit_shadowsocks.cpp
        ${NYXARIA}/ui/profile/edit_shadowsocks.ui
        ${NYXARIA}/ui/profile/edit_naive.h
        ${GHARQAD}/ui/profile/edit_naive.cpp
        ${NYXARIA}/ui/profile/edit_naive.ui
        ${NYXARIA}/ui/profile/edit_juicity.h
        ${GHARQAD}/ui/profile/edit_juicity.cpp
        ${NYXARIA}/ui/profile/edit_juicity.ui
        ${NYXARIA}/ui/profile/edit_trusttunnel.h
        ${GHARQAD}/ui/profile/edit_trusttunnel.cpp
        ${NYXARIA}/ui/profile/edit_trusttunnel.ui
        ${NYXARIA}/ui/profile/edit_vmess.h
        ${GHARQAD}/ui/profile/edit_vmess.cpp
        ${NYXARIA}/ui/profile/edit_vmess.ui
        ${NYXARIA}/ui/profile/edit_mieru.h
        ${GHARQAD}/ui/profile/edit_mieru.cpp
        ${NYXARIA}/ui/profile/edit_mieru.ui
        ${NYXARIA}/ui/profile/edit_tor.h
        ${GHARQAD}/ui/profile/edit_tor.cpp
        ${NYXARIA}/ui/profile/edit_tor.ui
        ${NYXARIA}/ui/profile/edit_trojan_vless.h
        ${GHARQAD}/ui/profile/edit_trojan_vless.cpp
        ${NYXARIA}/ui/profile/edit_trojan_vless.ui
        ${NYXARIA}/ui/profile/edit_anytls.h
        ${NYXARIA}/ui/profile/edit_shadowtls.h
        ${GHARQAD}/ui/profile/edit_anytls.cpp
        ${NYXARIA}/ui/profile/edit_anytls.ui
        ${NYXARIA}/ui/profile/edit_tailscale.h
        ${GHARQAD}/ui/profile/edit_tailscale.cpp
        ${NYXARIA}/ui/profile/edit_tailscale.ui

        ${NYXARIA}/ui/profile/edit_quic.h
        ${GHARQAD}/ui/profile/edit_quic.cpp
        ${NYXARIA}/ui/profile/edit_quic.ui

        ${NYXARIA}/ui/profile/edit_custom.h
        ${GHARQAD}/ui/profile/edit_custom.cpp
        ${NYXARIA}/ui/profile/edit_custom.ui

        ${NYXARIA}/ui/profile/edit_extra_core.h
        ${GHARQAD}/ui/profile/edit_extra_core.cpp
        ${NYXARIA}/ui/profile/edit_extra_core.ui

        ${NYXARIA}/ui/profile/edit_wireguard.h
        ${GHARQAD}/ui/profile/edit_wireguard.cpp
        ${NYXARIA}/ui/profile/edit_wireguard.ui

        ${GHARQAD}/ui/setting/dialog_basic_settings.cpp
        ${NYXARIA}/ui/setting/dialog_basic_settings.h
        ${NYXARIA}/ui/setting/dialog_basic_settings.ui

        ${GHARQAD}/ui/group/dialog_manage_groups.cpp
        ${NYXARIA}/ui/group/dialog_manage_groups.h
        ${NYXARIA}/ui/group/dialog_manage_groups.ui

        ${GHARQAD}/ui/setting/dialog_manage_routes.cpp
        ${NYXARIA}/ui/setting/dialog_manage_routes.h
        ${NYXARIA}/ui/setting/dialog_manage_routes.ui

        ${GHARQAD}/ui/setting/dialog_vpn_settings.cpp
        ${NYXARIA}/ui/setting/dialog_vpn_settings.h
        ${NYXARIA}/ui/setting/dialog_vpn_settings.ui

        ${GHARQAD}/ui/setting/dialog_hotkey.cpp
        ${NYXARIA}/ui/setting/dialog_hotkey.h
        ${NYXARIA}/ui/setting/dialog_hotkey.ui

        ${GHARQAD}/ui/profile/ProxyItem.cpp
        ${NYXARIA}/ui/profile/ProxyItem.h
        ${NYXARIA}/ui/profile/ProxyItem.ui
        ${GHARQAD}/ui/group/GroupItem.cpp
        ${NYXARIA}/ui/group/GroupItem.h
        ${NYXARIA}/ui/group/GroupItem.ui
        ${GHARQAD}/ui/setting/RouteItem.cpp
        ${NYXARIA}/ui/setting/RouteItem.h
        ${NYXARIA}/ui/setting/RouteItem.ui

        res/nekobox.qrc
        ${QV2RAY_RC}
        ${GHARQAD}/dataStore/RouteEntity.cpp
        ${GHARQAD}/dataStore/ResourceEntity.cpp
        res/darkstyle.qrc
        ${GHARQAD}/ui/profile/edit_ssh.cpp
        ${NYXARIA}/ui/profile/edit_ssh.h
        ${NYXARIA}/ui/profile/edit_ssh.ui
        ${GHARQAD}/sys/Settings.cpp

        ${GHARQAD}/sys/AutoRun.cpp
        ${GHARQAD}/sys/Process.cpp
        ${NYXARIA}/ui/mainwindow_interface.h
        ${NYXARIA}/stats/connections/connectionLister.hpp
        ${GHARQAD}/stats/connectionLister/connectionLister.cpp
        ${GHARQAD}/configs/proxy/Json2Bean.cpp
        ${GHARQAD}/dataStore/Group.cpp
        ${GHARQAD}/dataStore/ProxyEntity.cpp

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

if(SKIP_UPDATE_BUTTON)
    nkr_add_compile_definitions(SKIP_UPDATE_BUTTON=yes)
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
        Qt6::Widgets Qt6::Network 
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

if (MSVC)
    target_compile_options(nekobox PRIVATE "/std:c++20")       # add c++20 feature
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set_target_properties(nekobox PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

qt_finalize_executable(nekobox)

if (UNIX)
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

set(START "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/nekobox")
set(MAIN "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/nekobox/nekobox")
set(ICON "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps/nekobox.png")

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

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nekobox/public)
install(TARGETS nekobox DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nekobox)
install(FILES 
    ${CMAKE_SOURCE_DIR}/srslist.json 
    ${CMAKE_SOURCE_DIR}/check_routeprofiles.js 
    ${CMAKE_SOURCE_DIR}/res/languages.txt DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nekobox/public)

endif()
