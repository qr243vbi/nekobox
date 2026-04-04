set(NKR_VERSION "DYNAMIC" CACHE STRING "A custom version string for application")
# Check if INPUT_VERSION environment variable is defined
# Check if INPUT_VERSION environment variable is set

if(DEFINED ENV{INPUT_VERSION} AND NOT "$ENV{INPUT_VERSION}" STREQUAL "")
    set(NKR_DEFAULT_VERSION "$ENV{INPUT_VERSION}" CACHE STRING
        "A custom default version string for application")
else()
    set(NKR_DEFAULT_VERSION "1.0.0" CACHE STRING
        "A custom default version string for application")
endif()


string(TIMESTAMP CURRENT_DATE_TIME "%Y-%m-%d")
# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()

function(install_if_exists arg dest)
    if(EXISTS "${arg}")
        install(FILES "${arg}" DESTINATION "${dest}")
    endif()
endfunction()

function(remove_rpath target_name)
    find_program(CHRPATH_EXECUTABLE chrpath)

    if (CHRPATH_EXECUTABLE)
        message(STATUS "chrpath found: ${CHRPATH_EXECUTABLE}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CHRPATH_EXECUTABLE}" -d $<TARGET_FILE:${target_name}>
        )
    else()
        message(STATUS "chrpath not found")

        find_program(PATCHELF_EXECUTABLE patchelf)
        if (PATCHELF_EXECUTABLE)
            message(STATUS "patchelf found: ${PATCHELF_EXECUTABLE}")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${PATCHELF_EXECUTABLE}" --remove-rpath $<TARGET_FILE:${target_name}>
            )
        else()
            message(FATAL_ERROR "neither patchelf nor chrpath found")
        endif()
    endif()
endfunction()

nkr_add_compile_definitions(NKR_TIMESTAMP=\"${CURRENT_DATE_TIME}\")

# Check if the build type is Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    nkr_add_compile_definitions(DEBUG_MODE=\"console\")
endif()

if(NKR_VERSION STREQUAL "DYNAMIC")
    nkr_add_compile_definitions(NKR_DEFAULT_VERSION=\"${NKR_DEFAULT_VERSION}\")
else()
    nkr_add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")
endif()
# Release

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")


set(NKR_SOFTWARE_KEYS "0000" CACHE STRING "NekoBox keys")


if(NKR_SOFTWARE_KEYS STREQUAL "cm")
else()
include("cmake/nkr_security.cmake")

endif()

