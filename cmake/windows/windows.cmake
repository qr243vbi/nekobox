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
    if (NOT DEFINED MinGW_ROOT)
        set(MinGW_ROOT "C:/msys64/mingw64")
    endif ()
elseif (MSVC)
    add_compile_options(/permissive-)
    add_compile_options("/utf-8")
    add_compile_options("/wd4702")
endif ()

add_definitions(-D_WIN32_WINNT=0x600 -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN)

