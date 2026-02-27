
if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
        set(MACORO_CONFIG "x64-Release")
    else()
        set(MACORO_CONFIG "x64-${CMAKE_BUILD_TYPE}")
    endif()
elseif(APPLE)
    set(MACORO_CONFIG "osx")
else()
    set(MACORO_CONFIG "linux")
endif()



if(NOT DEFINED MACORO_STAGE)

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in)
        # we currenty are in the macoro source tree, macoro/cmake
		set(MACORO_STAGE "${CMAKE_CURRENT_LIST_DIR}/../out/install/${MACORO_CONFIG}")
    else()
        # we currenty are in install tree, <install-prefix>/lib/cmake/macoro
        set(MACORO_STAGE "${CMAKE_CURRENT_LIST_DIR}/../../..")
    endif()

    get_filename_component(MACORO_STAGE ${MACORO_STAGE} ABSOLUTE)
endif()

if(DEFINED MACORO_BUILD)
    message(STATUS "Option: MACORO_STAGE = ${MACORO_STAGE}")
endif()