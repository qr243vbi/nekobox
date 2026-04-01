cmake_minimum_required(VERSION 3.16)
project(go_multi_build)

# ----------------------------
# Platform variables
# ----------------------------

set(GOOS "" CACHE STRING "Target GOOS (e.g. linux, windows, darwin)")
set(GOARCH "" CACHE STRING "Target GOARCH (e.g. amd64, arm64, 386, arm)")

if (GOOS STREQUAL "")
    if(WIN32)
        set(GOOS "windows")
    else()
        set(GOOS "")
    endif()
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_SYSTEM_PROCESSOR_LOWERCASE)
message("Processor is " ${CMAKE_SYSTEM_PROCESSOR_LOWERCASE})

if (GOARCH STREQUAL "")
    if(${CMAKE_SYSTEM_PROCESSOR_LOWERCASE} MATCHES "x86_64|amd64")
        set(GOARCH "amd64")
    elseif(${CMAKE_SYSTEM_PROCESSOR_LOWERCASE} MATCHES "aarch64|arm64")
        set(GOARCH "arm64")
    elseif(${CMAKE_SYSTEM_PROCESSOR_LOWERCASE} MATCHES "armv7l|arm")
        set(GOARCH "arm")
    elseif(${CMAKE_SYSTEM_PROCESSOR_LOWERCASE} MATCHES "i386|i686|i586|i486")
        set(GOARCH "386")
    else()
        set(GOARCH "") # safe fallback
    endif()
endif()


message("GOOS is " ${GOOS})
message("GOARCH is " ${GOARCH})

if (NOT GO_SOURCE_DIR OR GO_SOURCE_DIR STREQUAL "")
    set(GO_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
endif()

set(DESTDIR "${CMAKE_BINARY_DIR}" CACHE STRING "Destination")

if (GOOS STREQUAL "windows")
    set(EXE_SUFFIX ".exe")
else()
    set(EXE_SUFFIX "")
endif()

set(ENV{GOOS} ${GOOS})
set(ENV{GOARCH} ${GOARCH})

# ----------------------------
# Options (boolean flags)
# ----------------------------
option(SKIP_UPDATER "Skip building updater" OFF)
option(GO_MOD_TIDY "Run go mod tidy before build" ON)

# ----------------------------
# Tooling
# ----------------------------
set(GOCMD "go" CACHE STRING "Go executable command")

# ----------------------------
# Offline build
# ----------------------------
set(GO_MOD_FLAG "")

if (GO_MOD_TIDY)
option(GO_MOD_VENDOR "Build Go with -mod=vendor" OFF)
else()
option(GO_MOD_VENDOR "Build Go with -mod=vendor" ON)
endif()

if(GO_MOD_VENDOR)
    set(GO_MOD_FLAG "-mod=vendor")
endif()

# optional tidy command
set(GO_TIDY_CMD "")
if(GO_MOD_TIDY)
    set(GO_TIDY_CMD ${GOCMD} mod tidy)
else()
    set(GO_TIDY_CMD "")
endif()
set(GO_VENDOR_CMD "")
if(GO_MOD_TIDY AND GO_MOD_VENDOR)
    set(GO_VENDOR_CMD ${GOCMD} mod vendor)
else()
    set(GO_VENDOR_CMD "")
endif()

message(STATUS "DESTINATION IS ${DESTDIR} FOR MACHINE ${GOARCH} with platform ${GOOS}")

# ----------------------------
# Tags
# ----------------------------
set(TAGS "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_shadowtls,with_grpc,with_acme")

if(GOARCH STREQUAL "arm64" OR GOARCH STREQUAL "amd64")
    set(TAGS "${TAGS},with_naive,with_naive_outbound,with_purego")
endif()

file(MAKE_DIRECTORY "${DESTDIR}")

function(run_go_command DIR)
    string(JOIN " " joined_args "cd" ${DIR} ";" ${ARGN})
    message(STATUS "${joined_args}")
    if (ARGN)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E chdir "${DIR}" ${ARGN}
            WORKING_DIRECTORY "${DIR}"
        )
    endif()
endfunction()

# ----------------------------
# Environment for Go
# ----------------------------
set(ENV{CGO_ENABLED} "0")
set(ENV{GOTOOLCHAIN} "local")

# ----------------------------
# Updater build (optional)
# ----------------------------
if(NOT SKIP_UPDATER)
    run_go_command("${GO_SOURCE_DIR}/updater" ${GO_TIDY_CMD})
    run_go_command("${GO_SOURCE_DIR}/updater" ${GO_VENDOR_CMD})

    add_custom_command(
        OUTPUT "${DESTDIR}/updater${EXE_SUFFIX}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${GO_SOURCE_DIR}/updater"
                "${GOCMD}" build "${GO_MOD_FLAG}" -o "${DESTDIR}/updater${EXE_SUFFIX}" -trimpath -ldflags "-w -s"
    )
    add_custom_target(updater ALL DEPENDS "${DESTDIR}/updater${EXE_SUFFIX}")

    if (UNIX)
        install(PROGRAMS "${DESTDIR}/updater${EXE_SUFFIX}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/nekobox/")
    endif()
endif()

# ----------------------------
# Core server build
# ----------------------------

# run gen/update_libs.sh
run_go_command("${GO_SOURCE_DIR}/server/gen" env bash update_libs.sh)

# get VERSION_SINGBOX
execute_process(
    COMMAND ${CMAKE_COMMAND} -E chdir "${GO_SOURCE_DIR}/server"
            ${GOCMD} list -m -f {{.Version}} github.com/sagernet/sing-box
    WORKING_DIRECTORY ${GO_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION_SINGBOX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(LDFLAGS
    "-w -s -X github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX} -X internal/godebug.defaultGODEBUG=multipathtcp=0 -checklinkname=0"
)

run_go_command("${GO_SOURCE_DIR}/server" ${GO_TIDY_CMD})
run_go_command("${GO_SOURCE_DIR}/server" ${GO_VENDOR_CMD})

add_custom_command(
    OUTPUT ${DESTDIR}/nekobox_core${EXE_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E chdir ${GO_SOURCE_DIR}/server
            ${GOCMD} build "${GO_MOD_FLAG}" -v -o ${DESTDIR}/nekobox_core${EXE_SUFFIX} -trimpath -ldflags "${LDFLAGS}" -tags "${TAGS}"
)

add_custom_target(core_server ALL DEPENDS "${DESTDIR}/nekobox_core${EXE_SUFFIX}")

if (UNIX)
    install(PROGRAMS "${DESTDIR}/nekobox_core${EXE_SUFFIX}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/nekobox/")
endif()
