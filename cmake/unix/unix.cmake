set(PLATFORM_SOURCES
    src/sys/linux/LinuxCap.cpp
    ${NEKOBOX_INCLUDE}/sys/linux/LinuxCap.h
)
find_package(Qt6 REQUIRED COMPONENTS DBus)
set(PLATFORM_LIBRARIES dl stdc++ m Qt6::DBus)
