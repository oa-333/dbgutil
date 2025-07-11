cmake_minimum_required(VERSION 3.13)
project(DBG_UTIL VERSION 1.0 DESCRIPTION "Debug Utilities Library" LANGUAGES CXX)

# build shared library
add_library(dbgutil SHARED)

if (MINGW)
    set(SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_mingw-${CMAKE_BUILD_TYPE}")
else()
    set(SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}")
endif()

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
    target_compile_definitions(dbgutil PRIVATE DBGUTIL_DLL)
    target_compile_options(dbgutil PRIVATE /Zi /EHsc)
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
set(DBGUTIL_INSTALL_REL_PATH ${CMAKE_INSTALL_PREFIX}/../bin/${SYSTEM_NAME})
cmake_path(NORMAL_PATH DBGUTIL_INSTALL_REL_PATH OUTPUT_VARIABLE DBGUTIL_INSTALL_PATH)
make_directory(${DBGUTIL_INSTALL_PATH})
install(TARGETS dbgutil LIBRARY DESTINATION ${DBGUTIL_INSTALL_PATH})
install(TARGETS dbgutil RUNTIME DESTINATION ${DBGUTIL_INSTALL_PATH})