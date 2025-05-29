cmake_minimum_required(VERSION 3.13)
project(DBG_UTIL VERSION 1.0 DESCRIPTION "Debug Utilities Library" LANGUAGES CXX)

# build shared library
add_library(dbgutil SHARED)

# Using C++ 20 standard on Windows, and C++ 23 on other platforms
if (MSVC)
    target_compile_features(dbgutil PRIVATE cxx_std_20)
else()
    target_compile_features(dbgutil PRIVATE cxx_std_23)
endif()

# common compile flags for all targets
if (MSVC)
    # /Zi - generate full debug information
    # /EHsc - catch(..) catches only C++ exceptions, extern "C" functions never throw C++ exception
    add_compile_definitions(-DDBGUTIL_DLL)
    add_compile_options(/Zi /EHsc)
    target_compile_options(dbgutil PRIVATE /wd4251)
    target_link_libraries(dbgutil Ws2_32 Advapi32 dbghelp psapi)
elseif(MINGW)
    target_link_libraries(dbgutil ws2_32 dbghelp psapi intl iberty zstd z sframe unwind)
else()
    target_link_libraries(dbgutil unwind)
    add_link_options(-rdynamic)
endif()

# include path
target_include_directories(dbgutil PRIVATE inc)

# source folders
add_subdirectory(src)

# public headers
add_subdirectory(inc)

# install header in install dir
install(TARGETS dbgutil FILE_SET publicheaders)

# common install dir
if (MSVC OR MINGW)
    install(TARGETS dbgutil RUNTIME DESTINATION "C:\\install\\bin")
else()
    install(TARGETS dbgutil RUNTIME DESTINATION ~/install/bin)
endif()