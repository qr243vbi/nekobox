set(NKR_VERSION "DYNAMIC" CACHE STRING "A custom version string for application")
set(NKR_DEFAULT_VERSION "1.0.0" CACHE STRING "A custom version string for application")

if(NKR_VERSION STREQUAL "DYNAMIC")
    add_compile_definitions(NKR_DYNAMIC_VERSION)
    add_compile_definitions(NKR_DEFAULT_VERSION=\"${NKR_DEFAULT_VERSION}\")
    list(APPEND PLATFORM_SOURCES include/js/version.h src/js/version.cpp)
else()
    add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")
endif()
# Release

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")

# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()
