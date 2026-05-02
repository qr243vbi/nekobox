cmake_minimum_required(VERSION 3.10)
project(nekobox_core VERSION 6.0.0)

# Use this snippet *after* PROJECT(xxx):
if(UNIX)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "default prefix" FORCE)
    endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    include(GNUInstallDirs)
endif()# ----------------------------

set(GO_ENV)
function(add_env_var KEY VALUE)
    # Optional: set for current process
    set(ENV{${KEY}} "${VALUE}")

    # Work on a local copy
    set(_env "${GO_ENV}")
    list(APPEND _env "${KEY}=${VALUE}")

    # Export back to parent
    set(GO_ENV "${_env}" PARENT_SCOPE)
endfunction()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# Platform variables
# ----------------------------
if(EXISTS "${GO_SOURCE_DIR}/server/vendor")
    set(GO_MOD_TIDY_FLAG OFF CACHE STRING "Run go mod tidy before build")
else()
    set(GO_MOD_TIDY_FLAG ON CACHE STRING "Run go mod tidy before build")
endif()

set(GOOS "" CACHE STRING "Target GOOS (e.g. linux, windows, darwin)")
set(GOARCH "" CACHE STRING "Target GOARCH (e.g. amd64, arm64, 386, arm)")

if (GOOS STREQUAL "")
    if(WIN32)
        set(GOOS "windows")
    elseif(LINUX)
        set(GOOS "linux")
    else()
        set(GOOS "")
    endif()
endif()


find_program(THRIFT_COMPILER thrift)

if(EXISTS "${THRIFT_COMPILER}/tools/thrift/thrift.exe" AND NOT IS_DIRECTORY "${THRIFT_COMPILER}/tools/thrift/thrift.exe")
     set(THRIFT_COMPILER "${THRIFT_COMPILER}/tools/thrift/thrift.exe")
     set(ENV{THRIFT_COMPILER} "${THRIFT_COMPILER}")
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

add_env_var("GOOS" "${GOOS}")
add_env_var("GOARCH" "${GOARCH}")

# ----------------------------
# Options (boolean flags)
# ----------------------------

set(GO_MOD_TIDY_FLAG OFF CACHE STRING "Run go mod tidy before build")
option(SKIP_UPDATER "Skip building updater" OFF)
option(GO_MOD_TIDY "Run go mod tidy before build" ${GO_MOD_TIDY_FLAG})
set(PROGRAMPREFIX "${CMAKE_INSTALL_LIBEXECDIR}/Iblis" CACHE STRING "Installation Directory")


# ----------------------------
# Tooling
# ----------------------------
find_program(GO_COMPILER go)
set(GOCMD "${GO_COMPILER}" CACHE STRING "Go executable command")
execute_process(
    COMMAND "${GOCMD}" env GOROOT
    OUTPUT_VARIABLE GOROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
	RESULT_VARIABLE GO_RESULT
)


if(NOT GO_RESULT EQUAL 0)
	get_filename_component(GO_BIN "${GOCMD}" ABSOLUTE)

    if(NOT EXISTS "${GO_BIN}")
        message(FATAL_ERROR "go binary not found: ${GO_BIN}")
    endif()

    # bin directory
    get_filename_component(GO_BIN_DIR "${GO_BIN}" DIRECTORY)
	
    # Resolve path (../lib/go relative to GO_BIN_DIR)
    get_filename_component(GOROOT "${GO_BIN_DIR}/../lib/go" ABSOLUTE)

    if(NOT EXISTS "${GOROOT}")
        message(FATAL_ERROR "Go root not found at ${GOROOT}")
    endif()

    set(VERSION_FILE "${GOROOT}/VERSION")
    set(ENV_FILE "${GOROOT}/go.env")

    if(NOT EXISTS "${VERSION_FILE}")
        message(FATAL_ERROR "Missing VERSION file in ${GOROOT}")
    endif()

    if(NOT EXISTS "${ENV_FILE}")
        message(FATAL_ERROR "Missing go.env file in ${GOROOT}")
    endif()
	
    message(STATUS "Found Go root: ${GOROOT}")
endif()

# ----------------------------
# Offline build
# ----------------------------
set(GO_MOD_FLAG "")

if (GO_MOD_TIDY)
set(GO_MOD_VENDOR_FLAG "OFF")
else()
set(GO_MOD_VENDOR_FLAG "ON")
endif()

option(GO_MOD_VENDOR "Build Go with -mod=vendor" ${GO_MOD_VENDOR_FLAG})

if(GO_MOD_VENDOR)
    set(GO_MOD_FLAG "-mod=vendor")
else()
    set(GO_MOD_FLAG "-mod=mod")
endif()

# optional tidy command
set(GO_TIDY_CMD "")
if(GO_MOD_TIDY)
    set(GO_TIDY_CMD "${GOCMD}" mod tidy)
else()
    set(GO_TIDY_CMD "")
endif()
set(GO_VENDOR_CMD "")
if(GO_MOD_TIDY AND GO_MOD_VENDOR)
    set(GO_VENDOR_CMD "${GOCMD}" mod vendor)
else()
    set(GO_VENDOR_CMD "")
endif()

message(STATUS "DESTINATION IS ${DESTDIR} FOR MACHINE ${GOARCH} with platform ${GOOS}")

# ----------------------------
# Tags
# ----------------------------
set(TAGS "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_shadowtls,with_grpc,with_acme,with_internal_resolvectl")

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
add_env_var("GOROOT" "${GOROOT}")
add_env_var("CGO_ENABLED" "1")
add_env_var("GOTOOLCHAIN" "local")

# ----------------------------
# Updater build (optional)
# ----------------------------
if(NOT SKIP_UPDATER)
    run_go_command("${GO_SOURCE_DIR}/updater" ${GO_TIDY_CMD})
    run_go_command("${GO_SOURCE_DIR}/updater" ${GO_VENDOR_CMD})

    add_custom_command(
        OUTPUT "${DESTDIR}/updater${EXE_SUFFIX}"
        COMMAND ${CMAKE_COMMAND} -E env ${GO_ENV} CGO_ENABLED=0
				${CMAKE_COMMAND} -E chdir "${GO_SOURCE_DIR}/updater"
                "${GOCMD}" build ${GO_MOD_FLAG} -o "${DESTDIR}/updater${EXE_SUFFIX}" -trimpath -ldflags "-w -s"
    )
    add_custom_target(updater ALL DEPENDS "${DESTDIR}/updater${EXE_SUFFIX}")

    if (UNIX)
        install(PROGRAMS "${DESTDIR}/updater${EXE_SUFFIX}" DESTINATION "${PROGRAMPREFIX}")
    endif()
endif()

# ----------------------------
# Core server build
# ----------------------------

# run gen/update_libs.sh
run_go_command("${GO_SOURCE_DIR}/server/gen" env "THRIFT_COMPILER=${THRIFT_COMPILER}" bash update_libs.sh)

# get VERSION_SINGBOX
execute_process(
    COMMAND ${CMAKE_COMMAND} -E chdir "${GO_SOURCE_DIR}/server"
            "${GOCMD}" list -m -f {{.Version}} github.com/sagernet/sing-box
    WORKING_DIRECTORY ${GO_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION_SINGBOX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(LDFLAGS
    "-w -s -X github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX} -X internal/godebug.defaultGODEBUG=multipathtcp=0 -checklinkname=0"
)

run_go_command("${GO_SOURCE_DIR}/server" ${GO_TIDY_CMD})
run_go_command("${GO_SOURCE_DIR}/server" ${GO_VENDOR_CMD})


if (GOOS STREQUAL "linux")
    # Define the source file (ensure wrapper.c is in your source directory)
    set(WRAPPER_SRC "${GO_SOURCE_DIR}/server/stub/elevated_resolvctl.c")

    if(EXISTS "${WRAPPER_SRC}")
        add_executable(nekobox_core_elevated_resolvectl "${WRAPPER_SRC}")

        # Apply optimizations and strip symbols
        target_compile_options(nekobox_core_elevated_resolvectl PRIVATE -O3)

        # Rename output to match 'resolvectl' if desired, or keep as wrapper
        set_target_properties(nekobox_core_elevated_resolvectl PROPERTIES OUTPUT_NAME "nekobox_core_elevated_resolvectl")

        set(WRAPPER_DEP_FILE $<TARGET_FILE:nekobox_core_elevated_resolvectl>)

        set(EMBED_PATH "${GO_SOURCE_DIR}/server/elevated_resolvctl")
        # Create a hardlink (removes existing file first to avoid 'File exists' errors)
        add_custom_command(TARGET nekobox_core_elevated_resolvectl POST_BUILD
            COMMAND strip --strip-all $<TARGET_FILE:nekobox_core_elevated_resolvectl>
            COMMENT "Stripping symbols from C wrapper..."
        )

        add_custom_command(TARGET nekobox_core_elevated_resolvectl POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nekobox_core_elevated_resolvectl> "${EMBED_PATH}"
            COMMENT "Copying wrapper to Go source for embedding"
        )


    else()
        message(WARNING "wrapper not found at ${WRAPPER_SRC}, skipping wrapper build.")
    endif()
endif()



if (UNIX)
    get_filename_component(MAIN_CORE "${PROGRAMPREFIX}/nekobox_core${EXE_SUFFIX}" REALPATH BASE_DIR "${CMAKE_INSTALL_PREFIX}")

    install(PROGRAMS "${DESTDIR}/nekobox_core${EXE_SUFFIX}" DESTINATION "${PROGRAMPREFIX}")
    install(PROGRAMS "${CMAKE_BINARY_DIR}/start_sing_box.sh" RENAME "sing-box" DESTINATION "${CMAKE_INSTALL_BINDIR}")

    # Configure the desktop file
    configure_file(
        "${GO_SOURCE_DIR}/start_sing_box.sh.in"
        "${CMAKE_BINARY_DIR}/start_sing_box.sh"
        @ONLY
    )
endif()


add_custom_command(
    OUTPUT ${DESTDIR}/nekobox_core${EXE_SUFFIX}
    DEPENDS ${WRAPPER_DEP_FILE}
    COMMAND ${CMAKE_COMMAND} -E env ${GO_ENV}
			${CMAKE_COMMAND} -E chdir ${GO_SOURCE_DIR}/server
            "${GOCMD}" build ${GO_MOD_FLAG} -buildmode=pie -v -o ${DESTDIR}/nekobox_core${EXE_SUFFIX} -trimpath -ldflags "${LDFLAGS}" -tags "${TAGS}"
)

add_custom_target(core_server ALL DEPENDS "${DESTDIR}/nekobox_core${EXE_SUFFIX}")

