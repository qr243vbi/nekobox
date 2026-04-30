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

function(embeed_resources)
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
	# Directory containing resources
	set(STATIC_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/res/public")
	set(QRC_FILE "${CMAKE_BINARY_DIR}/nekoobox_static.qrc")
	# Get all files in the directory
	file(GLOB_RECURSE STATIC_RESOURCE_FILES "${STATIC_RESOURCE_DIR}/*")
	# Generate .qrc file
	file(WRITE ${QRC_FILE} "<RCC>\n  <qresource prefix=\"/\">\n")
	foreach(FILE_PATH IN LISTS STATIC_RESOURCE_FILES)
		# Make path relative to project root
		file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/res/public" "${FILE_PATH}")
		file(COPY "${FILE_PATH}" DESTINATION "${CMAKE_BINARY_DIR}/")
		file(APPEND ${QRC_FILE} "    <file>${RELATIVE_PATH}</file>\n")
	endforeach()
	foreach(FILE_PATH IN LISTS QM_FILENAMES)
		# Make path relative to project root
		file(APPEND ${QRC_FILE} "    <file>${FILE_PATH}.qm</file>\n")
	endforeach()
	file(COPY "${CMAKE_SOURCE_DIR}/srslist.json" DESTINATION "${CMAKE_BINARY_DIR}")
	file(COPY "${CMAKE_SOURCE_DIR}/res/languages.txt" DESTINATION "${CMAKE_BINARY_DIR}")
	file(APPEND ${QRC_FILE} "    <file>srslist.json</file>\n")
	file(APPEND ${QRC_FILE} "    <file>languages.txt</file>\n")
	file(APPEND ${QRC_FILE} "  </qresource>\n</RCC>\n")
	set(PLATFORM_SOURCES ${PLATFORM_SOURCES} "${QRC_FILE}")
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

