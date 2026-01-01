cmake_minimum_required(VERSION 3.10)

project(nekobox VERSION 5.8.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
	if (NOT MSVC)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
	endif ()
endif ()

set(NEKOBOX_INCLUDE "src/nekobox")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network LinguistTools DBus)

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

include("cmake/print.cmake")
include("cmake/nkr.cmake")
include("cmake/QHotkey.cmake")

find_package(Threads)

option(SKIP_UPDATE_BUTTON "Always hide update button" OFF)
option(SKIP_JS_UPDATER "Skip JS update checker" OFF)

if (NKR_PACKAGE)
    nkr_add_compile_definitions(NKR_CPP_USE_APPDATA)
endif ()

set(BUILD_SHARED_LIBS OFF)
list(APPEND NKR_EXTERNAL_TARGETS qhotkey)

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
    "src/libcore.cpp"
 #   "${THRIFT_GEN_DIR}/LibcoreService.cpp"
)


# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}
        ${THRIFT_GENERATED_SOURCES}
        src/main.cpp
        src/dataStore/Configs.cpp
        src/dataStore/Utils.cpp
        src/global/GuiUtils.cpp
        src/global/HTTPRequestHelper.cpp
        src/global/DeviceDetailsHelper.cpp
        src/global/CountryHelper.cpp

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

        src/api/RPC.cpp
        src/dataStore/Database.cpp
        src/stats/traffic/TrafficLooper.cpp
        src/dataStore/ProfileFilter.cpp
        src/configs/ConfigBuilder.cpp

        src/configs/proxy/AbstractBean.cpp
        src/configs/proxy/Bean2CoreObj_box.cpp
        src/configs/proxy/Bean2Link.cpp
        src/configs/proxy/Link2Bean.cpp
        ${NEKOBOX_INCLUDE}/configs/proxy/ChainBean.hpp # translate

        ${NEKOBOX_INCLUDE}/configs/sub/GroupUpdater.hpp
        src/configs/sub/GroupUpdater.cpp

        ${NEKOBOX_INCLUDE}/sys/Process.hpp
        src/sys/Process.cpp
        src/sys/AutoRun.cpp

        ${NEKOBOX_INCLUDE}/ui/setting/ThemeManager.hpp
        src/ui/setting/ThemeManager.cpp
        src/ui/setting/Icon.cpp

        src/ui/mainwindow_rpc.cpp
        src/ui/mainwindow.cpp
        ${NEKOBOX_INCLUDE}/ui/mainwindow.h
        ${NEKOBOX_INCLUDE}/ui/mainwindow.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_shadowtls.ui

        ${NEKOBOX_INCLUDE}/ui/profile/dialog_edit_profile.h
        src/ui/profile/dialog_edit_profile.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/dialog_edit_profile.ui
        ${NEKOBOX_INCLUDE}/ui/group/dialog_edit_group.h
        src/ui/group/dialog_edit_group.cpp
        ${NEKOBOX_INCLUDE}/ui/group/dialog_edit_group.ui
        ${NEKOBOX_INCLUDE}/ui/group/dialog_group_choose_proxy.ui

        ${NEKOBOX_INCLUDE}/ui/profile/edit_chain.h
        src/ui/profile/edit_chain.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_chain.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_socks_http.h
        src/ui/profile/edit_socks_http.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_socks_http.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_shadowsocks.h
        src/ui/profile/edit_shadowsocks.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_shadowsocks.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_vmess.h
        src/ui/profile/edit_vmess.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_vmess.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_mieru.h
        src/ui/profile/edit_mieru.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_mieru.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_trojan_vless.h
        src/ui/profile/edit_trojan_vless.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_trojan_vless.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_anytls.h
        ${NEKOBOX_INCLUDE}/ui/profile/edit_shadowtls.h
        src/ui/profile/edit_anytls.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_anytls.ui
        ${NEKOBOX_INCLUDE}/ui/profile/edit_tailscale.h
        src/ui/profile/edit_tailscale.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_tailscale.ui
        ${NEKOBOX_INCLUDE}/configs/proxy/Tailscale.hpp

        ${NEKOBOX_INCLUDE}/ui/profile/edit_quic.h
        src/ui/profile/edit_quic.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_quic.ui

        ${NEKOBOX_INCLUDE}/ui/profile/edit_custom.h
        src/ui/profile/edit_custom.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_custom.ui

        ${NEKOBOX_INCLUDE}/ui/profile/edit_extra_core.h
        src/ui/profile/edit_extra_core.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_extra_core.ui

        ${NEKOBOX_INCLUDE}/ui/profile/edit_wireguard.h
        src/ui/profile/edit_wireguard.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_wireguard.ui

        src/ui/setting/dialog_basic_settings.cpp
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_basic_settings.h
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_basic_settings.ui

        src/ui/group/dialog_manage_groups.cpp
        ${NEKOBOX_INCLUDE}/ui/group/dialog_manage_groups.h
        ${NEKOBOX_INCLUDE}/ui/group/dialog_manage_groups.ui

        src/ui/setting/dialog_manage_routes.cpp
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_manage_routes.h
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_manage_routes.ui

        src/ui/setting/dialog_vpn_settings.cpp
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_vpn_settings.h
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_vpn_settings.ui

        src/ui/setting/dialog_hotkey.cpp
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_hotkey.h
        ${NEKOBOX_INCLUDE}/ui/setting/dialog_hotkey.ui

        src/ui/profile/ProxyItem.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/ProxyItem.h
        ${NEKOBOX_INCLUDE}/ui/profile/ProxyItem.ui
        src/ui/group/GroupItem.cpp
        ${NEKOBOX_INCLUDE}/ui/group/GroupItem.h
        ${NEKOBOX_INCLUDE}/ui/group/GroupItem.ui
        src/ui/setting/RouteItem.cpp
        ${NEKOBOX_INCLUDE}/ui/setting/RouteItem.h
        ${NEKOBOX_INCLUDE}/ui/setting/RouteItem.ui

        res/nekobox.qrc
        ${QV2RAY_RC}
        ${NEKOBOX_INCLUDE}/dataStore/RouteEntity.h
        src/dataStore/RouteEntity.cpp
        src/dataStore/ResourceEntity.cpp
        res/darkstyle.qrc
        src/ui/profile/edit_ssh.cpp
        ${NEKOBOX_INCLUDE}/ui/profile/edit_ssh.h
        ${NEKOBOX_INCLUDE}/ui/profile/edit_ssh.ui
        ${NEKOBOX_INCLUDE}/configs/proxy/SSHBean.h
        src/sys/Settings.cpp

        src/sys/AutoRun.cpp
        src/sys/Process.cpp
        ${NEKOBOX_INCLUDE}/ui/mainwindow_interface.h
        ${NEKOBOX_INCLUDE}/stats/connections/connectionLister.hpp
        src/stats/connectionLister/connectionLister.cpp
        src/configs/proxy/Json2Bean.cpp
        ${NEKOBOX_INCLUDE}/sys/windows/eventHandler.h
        src/dataStore/Group.cpp
        src/dataStore/ProxyEntity.cpp

        src/stats/autotester/ProxyAutoTester.cpp
        ${NEKOBOX_INCLUDE}/stats/autotester/ProxyAutoTester.hpp
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
    target_sources(nekobox PRIVATE src/js/js_updater.cpp ${NEKOBOX_INCLUDE}/js/js_updater.h res/nekobox_updater.qrc)
endif()


target_include_directories(nekobox PRIVATE
    ${Qt6Protobuf_PRIVATE_INCLUDE_DIRS}
)

set_target_properties(nekobox PROPERTIES
        WIN32_EXECUTABLE TRUE
)

# Target Source Translations

set(TS_FILES
        res/translations/zh_CN.ts
        res/translations/fa_IR.ts
        res/translations/ru_RU.ts
)
set(LUPDATE_OPTIONS
        -locations none -no-obsolete
)
qt_add_lupdate(nekobox TS_FILES ${TS_FILES} OPTIONS ${LUPDATE_OPTIONS})
qt_add_lrelease(nekobox TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)

configure_file(res/translations/translations.qrc ${CMAKE_BINARY_DIR} COPYONLY)
target_sources(nekobox PRIVATE ${CMAKE_BINARY_DIR}/translations.qrc)

if (MSVC)
find_package(Thrift REQUIRED)
set(THRIFT_LIBRARIES thrift::thrift)
else()
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT thrift REQUIRED)
endif()

target_include_directories(nekobox PRIVATE ${THRIFT_INCLUDE_DIRS})
target_link_libraries(nekobox PRIVATE ${THRIFT_LIBRARIES})

# Target Link

target_link_libraries(nekobox PRIVATE
        Qt6::Widgets Qt6::Network Qt6::DBus
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

if (MSVC)
    target_compile_options(nekobox PRIVATE "/std:c++20")       # add c++20 feature
endif ()

qt_finalize_executable(nekobox)


install(TARGETS nekobox DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nekobox)
install(FILES res/public/icon.png check_routeprofiles.js res/public/emoji.ttf DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nekobox/public)
