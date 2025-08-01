cmake_minimum_required(VERSION 3.13)

#############################################################
# project definition
#############################################################
project(libdbg VERSION 0.1.2 DESCRIPTION "Debug Utilities Library" LANGUAGES CXX)

# build shared library
add_library(libdbg SHARED)
set_target_properties(libdbg PROPERTIES POSITION_INDEPENDENT_CODE ON)

#############################################################
# C++ version, should be inherited from outer project
# Otherwise defaults to C++ 11
#############################################################

if (DEFINED CMAKE_CXX_STANDARD)
    message(STATUS "libdbg Using externally defined CXX standard: ${CMAKE_CXX_STANDARD}")
    target_compile_features(libdbg PRIVATE ${CMAKE_CXX_STANDARD})
else()
    message(STATUS "CXX standard not defined, libdbg defaulting to cxx_std_11")
    target_compile_features(libdbg PRIVATE cxx_std_11)
endif()

#############################################################
# Compile flags
#############################################################
if (MSVC)
    # /Zi - generate full debug information
    # /EHsc - catch(..) catches only C++ exceptions, extern "C" functions never throw C++ exception
    target_compile_definitions(libdbg PRIVATE LIBDBG_DLL)
    target_compile_options(libdbg PRIVATE /Zi /EHsc)
    target_compile_options(libdbg PRIVATE /Wall)
    target_compile_options(libdbg PRIVATE /wd4251)
    target_compile_options(libdbg PRIVATE /wd4820)
    target_compile_options(libdbg PRIVATE /wd5045)
    target_compile_options(libdbg PRIVATE /wd5039)
    target_link_libraries(libdbg Ws2_32 dbghelp psapi)
elseif(MINGW)
    target_link_libraries(libdbg ws2_32 dbghelp psapi unwind)
    target_compile_options(libdbg PRIVATE -Wall)
else()
    target_link_libraries(libdbg unwind)
    target_compile_options(libdbg PRIVATE -Wall)
    add_link_options(-rdynamic)
endif()

#############################################################
# include path
#############################################################
target_include_directories(libdbg PRIVATE inc)

#############################################################
# source folders
#############################################################
add_subdirectory(src)

#############################################################
# local install for tests
#############################################################
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(STATUS "Installing at ${CMAKE_INSTALL_PREFIX}")

    # public headers
    add_subdirectory(inc)

    # install headers in install dir (irrelevant to fetch content)
    install(TARGETS libdbg FILE_SET publicheaders)
    
    # unique system name
    if (MINGW)
        set(SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_mingw-${CMAKE_BUILD_TYPE}")
    else()
        set(SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}")
    endif()

    # install in common bin dir for other projects (irrelevant to fetch content)
    set(DBGUTIL_INSTALL_REL_PATH ${CMAKE_INSTALL_PREFIX}/../bin/${SYSTEM_NAME})
    cmake_path(NORMAL_PATH DBGUTIL_INSTALL_REL_PATH OUTPUT_VARIABLE DBGUTIL_INSTALL_PATH)
    make_directory(${DBGUTIL_INSTALL_PATH})
    install(TARGETS libdbg LIBRARY DESTINATION ${DBGUTIL_INSTALL_PATH})
    install(TARGETS libdbg RUNTIME DESTINATION ${DBGUTIL_INSTALL_PATH})
endif()