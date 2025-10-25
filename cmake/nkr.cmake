if("$ENV{INPUT_VERSION}" STREQUAL "")
    set(NKR_VERSION "getNkrVersion")
    add_compile_definitions(NKR_VERSION=${NKR_VERSION})
else()
    set(NKR_VERSION "$ENV{INPUT_VERSION}")
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
