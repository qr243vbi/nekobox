set(PLATFORM_SOURCES 
    3rdparty/WinCommander.cpp 
    ${GHARQAD}/sys/windows/guihelper.cpp 
    ${GHARQAD}/sys/windows/MiniDump.cpp 
    ${GHARQAD}/sys/windows/eventHandler.cpp 
    ${GHARQAD}/sys/windows/WinVersion.cpp)
set(PLATFORM_LIBRARIES wininet wsock32 ws2_32 user32 rasapi32 iphlpapi ntdll wbemuuid)

include(cmake/windows/generate_product_version.cmake)
generate_product_version(
        QV2RAY_RC
        ICON "${CMAKE_SOURCE_DIR}/res/nekobox.ico"
        NAME "nekobox"
        BUNDLE "Iblis"
        COMPANY_NAME "Iblis Corporation"
        COMPANY_COPYRIGHT "nekobox"
        FILE_DESCRIPTION "nekobox"
)
add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
set(GUI_TYPE WIN32)
if (MINGW)
	# Directory containing resources
	set(STATIC_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/res/public")
	set(QRC_FILE "${CMAKE_BINARY_DIR}/nekoobox_static.qrc")
	# Get all files in the directory
	file(GLOB_RECURSE STATIC_RESOURCE_FILES "${STATIC_RESOURCE_DIR}/*")
	# Generate .qrc file
	file(WRITE ${QRC_FILE} "<RCC>\n  <qresource prefix=\"/nekobox/\">\n")
	foreach(FILE_PATH IN LISTS STATIC_RESOURCE_FILES)
		# Make path relative to project root
		file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/res" "${FILE_PATH}")
		file(RELATIVE_PATH REL_FILE_PATH "${CMAKE_BINARY_DIR}" "${FILE_PATH}")
		file(APPEND ${QRC_FILE} "    <file alias=\"${REL_PATH}\">${REL_FILE_PATH}</file>\n")
	endforeach()
	foreach(FILE_PATH IN LISTS QM_FILENAMES)
		# Make path relative to project root
		file(APPEND ${QRC_FILE} "    <file alias=\"public/${FILE_PATH}.qm\">${FILE_PATH}.qm</file>\n")
	endforeach()
	file(RELATIVE_PATH SRSLIST_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/srslist.json")
	file(RELATIVE_PATH LANGUAGES_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/res/languages.txt")
	file(APPEND ${QRC_FILE} "    <file alias=\"public/srslist.json\">${SRSLIST_PATH}</file>\n")
	file(APPEND ${QRC_FILE} "    <file alias=\"public/languages.txt\">${LANGUAGES_PATH}</file>\n")
	file(APPEND ${QRC_FILE} "  </qresource>\n</RCC>\n")
	set(PLATFORM_SOURCES ${PLATFORM_SOURCES} "${QRC_FILE}")
    add_compile_options("-static")
    if (NOT DEFINED MinGW_ROOT)
        set(MinGW_ROOT "C:/msys64/mingw64")
    endif ()
elseif (MSVC)
    add_compile_options(/permissive-)
    add_compile_options("/utf-8")
    add_compile_options("/wd4702")
endif ()

add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)

