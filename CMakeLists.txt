cmake_minimum_required(VERSION 3.13)

#############################################################
# project definition
#############################################################
project(dbgutil VERSION 1.0 DESCRIPTION "Debug Utilities Library" LANGUAGES CXX)

# build shared library
add_library(dbgutil SHARED)
set_target_properties(dbgutil PROPERTIES POSITION_INDEPENDENT_CODE ON)

#############################################################
# C++ version, should be inherited from outer project
# Otherwise defaults to C++ 11
#############################################################

if (DEFINED CMAKE_CXX_STANDARD)
    message(STATUS "dbgutil Using externally defined CXX standard: ${CMAKE_CXX_STANDARD}")
else()
    message(STATUS "CXX standard not defined, dbgutil defaulting to cxx_std_11")
    target_compile_features(dbgutil PUBLIC cxx_std_11)
endif()

#############################################################
# Compile flags
#############################################################
if (MSVC)
    # /Zi - generate full debug information
    # /EHsc - catch(..) catches only C++ exceptions, extern "C" functions never throw C++ exception
    target_compile_definitions(dbgutil PRIVATE DBGUTIL_DLL)
    target_compile_options(dbgutil PRIVATE /Zi /EHsc)
    target_compile_options(dbgutil PRIVATE /Wall)
    target_compile_options(dbgutil PRIVATE /wd4251)
    target_compile_options(dbgutil PRIVATE /wd4820)
    target_compile_options(dbgutil PRIVATE /wd5045)
    target_compile_options(dbgutil PRIVATE /wd5039)
    target_link_libraries(dbgutil Ws2_32 dbghelp psapi)
elseif(MINGW)
    target_link_libraries(dbgutil ws2_32 dbghelp psapi unwind)
    target_compile_options(dbgutil PRIVATE -Wall)
else()
    target_link_libraries(dbgutil unwind)
    target_compile_options(dbgutil PRIVATE -Wall)
    add_link_options(-rdynamic)
endif()

#############################################################
# include path
#############################################################
target_include_directories(dbgutil PRIVATE inc)

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
    install(TARGETS dbgutil FILE_SET publicheaders)
    
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
    install(TARGETS dbgutil LIBRARY DESTINATION ${DBGUTIL_INSTALL_PATH})
    install(TARGETS dbgutil RUNTIME DESTINATION ${DBGUTIL_INSTALL_PATH})
endif()