
file(GLOB_RECURSE PLATFORM_SOURCES_GLOB
    ${GHARQAD}/sys/windows/*.cpp
    ${NYXARIA}/sys/windows/*.hpp
    ${NYXARIA}/sys/windows/*.h
)
set(PLATFORM_SOURCES ${PLATFORM_SOURCES_GLOB})

set(PLATFORM_LIBRARIES wininet wsock32 ws2_32 user32 rasapi32 iphlpapi ntdll wbemuuid)
include(cmake/windows/generate_product_version.cmake)
generate_product_version(
        QV2RAY_RC
        ICON "${CMAKE_SOURCE_DIR}/res/nekobox.ico"
        NAME "nekoray"
        BUNDLE "NekoRay"
        COMPANY_NAME "NekoRay"
        COMPANY_COPYRIGHT "NekoRay"
        FILE_DESCRIPTION "NekoRay"
)
set(GUI_TYPE WIN32)
if (MINGW)
    embeed_resources()
#    add_compile_options("-static")
#    set(CMAKE_EXE_LINKER_FLAGS "-static")
    if (NOT DEFINED MinGW_ROOT)
        set(MinGW_ROOT "C:/msys64/mingw64")
    endif ()
elseif (MSVC)
    add_compile_options(/permissive-)
    add_compile_options("/utf-8")
    add_compile_options("/wd4702")
endif ()

