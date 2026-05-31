

file(GLOB_RECURSE PLATFORM_SOURCES_GLOB
    ${GHARQAD}/sys/unix/*.cpp
    ${NYXARIA}/sys/unix/*.hpp
    ${NYXARIA}/sys/unix/*.h
)
set(PLATFORM_SOURCES ${PLATFORM_SOURCES_GLOB})

find_package(Qt6 REQUIRED COMPONENTS DBus)
set(PLATFORM_LIBRARIES dl stdc++ m Qt6::DBus)
