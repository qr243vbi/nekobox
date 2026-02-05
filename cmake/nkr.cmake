set(NKR_VERSION "DYNAMIC" CACHE STRING "A custom version string for application")
# Check if INPUT_VERSION environment variable is defined
set(NKR_DEFAULT_VERSION "1.0.0" CACHE STRING "A custom default version string for application")

string(TIMESTAMP CURRENT_DATE_TIME "%Y-%m-%d")
# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()

nkr_add_compile_definitions(NKR_TIMESTAMP=\"${CURRENT_DATE_TIME}\")

if(NKR_VERSION STREQUAL "DYNAMIC")
    nkr_add_compile_definitions(NKR_DEFAULT_VERSION=\"${NKR_DEFAULT_VERSION}\")
else()
    nkr_add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")
endif()
# Release

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")


set(NKR_SOFTWARE_KEYS "cm" CACHE STRING "NekoBox keys")


if(NKR_SOFTWARE_KEYS STREQUAL "cm")
else()
include("cmake/nkr_security.cmake")

endif()

