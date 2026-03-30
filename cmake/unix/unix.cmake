set(PLATFORM_SOURCES
    ${GHARQAD}/sys/linux/LinuxCap.cpp
    ${NYXARIA}/sys/linux/LinuxCap.h
)
find_package(Qt6 REQUIRED COMPONENTS DBus)
set(PLATFORM_LIBRARIES dl stdc++ m Qt6::DBus)
