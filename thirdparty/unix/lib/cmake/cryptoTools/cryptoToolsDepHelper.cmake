cmake_policy(PUSH)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0045 NEW)
cmake_policy(SET CMP0074 NEW)



if(MSVC)
    if(NOT DEFINED CMAKE_BUILD_TYPE)
        set(OC_BUILD_TYPE "Release")
    elseif(MSVC AND ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
        set(OC_BUILD_TYPE "Release")
    else()
        set(OC_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    endif()

    set(OC_CONFIG "x64-${OC_BUILD_TYPE}")
elseif(APPLE)
    set(OC_CONFIG "osx")
else()
    set(OC_CONFIG "linux")
endif()


if(NOT DEFINED OC_THIRDPARTY_HINT)

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/cryptoToolsFindBuildDir.cmake)
        # we currenty are in the cryptoTools source tree, cryptoTools/cmake
        set(OC_THIRDPARTY_HINT "${CMAKE_CURRENT_LIST_DIR}/../out/install/${OC_CONFIG}")
        
        if(NOT DEFINED OC_THIRDPARTY_INSTALL_PREFIX)
        
		get_filename_component(OC_THIRDPARTY_INSTALL_PREFIX ${OC_THIRDPARTY_HINT} ABSOLUTE)
        endif()
    else()
        # we currenty are in install tree, <install-prefix>/lib/cmake/cryptoTools
        set(OC_THIRDPARTY_HINT "${CMAKE_CURRENT_LIST_DIR}/../../..")
    endif()
endif()


if(NOT OC_THIRDPARTY_CLONE_DIR)
    set(OC_THIRDPARTY_CLONE_DIR "${CMAKE_CURRENT_LIST_DIR}/../out")
endif()

set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${OC_THIRDPARTY_HINT};${CMAKE_PREFIX_PATH}")


## Relic
###########################################################################

macro(FIND_RELIC)
    set(ARGS ${ARGN})
    if(FETCH_RELIC)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    elseif(${NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()

    find_path(RLC_INCLUDE_DIR "relic/relic.h" PATH_SUFFIXES "/include/" ${ARGS})
    find_library(RLC_LIBRARY NAMES relic_s  PATH_SUFFIXES "/lib/" ${ARGS})
    if(EXISTS ${RLC_INCLUDE_DIR} AND EXISTS ${RLC_LIBRARY})
        set(RELIC_FOUND ON)
    else() 
        set(RELIC_FOUND OFF) 
    endif()
endmacro()
    
if(FETCH_RELIC_IMPL)
    FIND_RELIC()
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getRelic.cmake)
endif()


if (ENABLE_RELIC)
    
    FIND_RELIC()

    if(NOT RELIC_FOUND)
        message(FATAL_ERROR "could not find relic. Add -DFETCH_RELIC=ON or -DFETCH_ALL=ON to auto download.\n")
    endif()


    if(NOT TARGET relic)
        # does not property work on windows. Need to do a PR on relic.
        #find_package(RELIC REQUIRED HINTS "${OC_THIRDPARTY_HINT}")
        add_library(relic STATIC IMPORTED)
    
        set_property(TARGET relic PROPERTY IMPORTED_LOCATION ${RLC_LIBRARY})
        target_include_directories(relic INTERFACE 
                        $<BUILD_INTERFACE:${RLC_INCLUDE_DIR}>
                        $<INSTALL_INTERFACE:>)
    endif()
    message(STATUS "Relic_LIB:  ${RLC_LIBRARY}")
    message(STATUS "Relic_inc:  ${RLC_INCLUDE_DIR}\n")

endif()

# libsodium
###########################################################################

macro(FIND_SODIUM)
    set(ARGS ${ARGN})
    if(FETCH_SODIUM)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    elseif(${NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    find_path(SODIUM_INCLUDE_DIRS sodium.h PATH_SUFFIXES "/include/" ${ARGS})

    if(MSVC)
        set(SODIUM_LIB_NAME libsodium.lib)
    else()
        set(SODIUM_LIB_NAME libsodium.a)
    endif()
    
    find_library(SODIUM_LIBRARIES NAMES ${SODIUM_LIB_NAME} PATH_SUFFIXES "/lib/" ${ARGS})
    if(EXISTS ${SODIUM_INCLUDE_DIRS} AND EXISTS ${SODIUM_LIBRARIES})
        get_filename_component(SODIUM_LIBRARIES_DIR ${SODIUM_LIBRARIES} DIRECTORY)
        if(EXISTS ${SODIUM_LIBRARIES_DIR}/cmake/libsodium/libsodiumConfig.cmake)
            include(${SODIUM_LIBRARIES_DIR}/cmake/libsodium/libsodiumConfig.cmake)
            if((libsodium_pic AND ENABLE_PIC) OR ((NOT libsodium_pic) AND (NOT ENABLE_PIC)))
                set(SODIUM_FOUND  ON)
            else()
                message("Found incompatible libsodium at ${SODIUM_LIBRARIES_DIR}. ENABLE_PIC=${ENABLE_PIC}, libsodium_pic=${libsodium_pic}")
                set(SODIUM_FOUND  OFF) 
            endif()
        else()
            set(SODIUM_FOUND  ON)
        endif()
    else() 
        set(SODIUM_FOUND  OFF) 
    endif()

endmacro()

if(FETCH_SODIUM_IMPL)
    FIND_SODIUM()
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getSodium.cmake)
endif()

if (ENABLE_SODIUM)
  
    FIND_SODIUM()

    if (NOT SODIUM_FOUND)
        message(FATAL_ERROR "Failed to find libsodium.\n Add -DFETCH_SODIUM=ON or -DFETCH_ALL=ON to auto download.")
    endif ()
    
    set(SODIUM_MONTGOMERY ON CACHE BOOL "SODIUM_MONTGOMERY...")

    message(STATUS "SODIUM_INCLUDE_DIRS:  ${SODIUM_INCLUDE_DIRS}")
    message(STATUS "SODIUM_LIBRARIES:  ${SODIUM_LIBRARIES}")
    message(STATUS "SODIUM_MONTGOMERY:  ${SODIUM_MONTGOMERY}\n")

    if(NOT TARGET sodium)
        add_library(sodium STATIC IMPORTED)
    
        set_property(TARGET sodium PROPERTY IMPORTED_LOCATION ${SODIUM_LIBRARIES})
        target_include_directories(sodium INTERFACE 
            $<BUILD_INTERFACE:${SODIUM_INCLUDE_DIRS}>
            $<INSTALL_INTERFACE:>)


        if(MSVC)
            target_compile_definitions(sodium INTERFACE SODIUM_STATIC=1)
        endif()
    endif()
endif (ENABLE_SODIUM)

## coproto
###########################################################################


macro(FIND_COPROTO)
    if(FETCH_COPROTO)
        set(COPROTO_DP NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    elseif(${NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    else()
        unset(COPROTO_DP)
    endif()
    
    if(ENABLE_BOOST)
        set(COPROTO_COMPONENTS boost)
    else()
        set(COPROTO_COMPONENTS no_boost)
    endif()
    if(ENABLE_OPENSSL)
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} openssl)
    else()
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} no_openssl)
    endif()
    if(ENABLE_ASAN)
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} asan)
    else()
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} no_asan)
    endif()
    if(ENABLE_PIC)
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} pic)
    else()
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} no_pic)
    endif()

    if(DEFINED CRYPTO_TOOLS_STD_VER)
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} cpp${CRYPTO_TOOLS_STD_VER})
    endif()
    if(    "${CRYPTO_TOOLS_STD_VER}" STREQUAL "Release"
        OR "${CRYPTO_TOOLS_STD_VER}" STREQUAL "Debug"
        OR "${CRYPTO_TOOLS_STD_VER}" STREQUAL "RelWithDebInfo" )
        set(COPROTO_COMPONENTS ${COPROTO_COMPONENTS} ${CRYPTO_TOOLS_STD_VER} )
    endif()

    find_package(coproto ${COPROTO_DP} ${ARGN} COMPONENTS ${COPROTO_COMPONENTS})
endmacro()

if(ENABLE_COPROTO)

    if(FETCH_COPROTO_IMPL)
        FIND_COPROTO(QUIET)
        include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getCoproto.cmake)
    endif()

    FIND_COPROTO(REQUIRED)
endif()


## Span
###########################################################################

macro(FIND_SPAN)
    set(ARGS ${ARGN})
    if(FETCH_SPAN_LITE)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    elseif(${NO_SYSTEM_PATH})
        list(APPEND ARGS NO_CMAKE_SYSTEM_PATH)
    endif()
    find_package(span-lite ${ARGS})
endmacro()
    
if (FETCH_SPAN_LITE_IMPL)
    FIND_SPAN(QUIET)
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getSpanLite.cmake")
endif()

FIND_SPAN(REQUIRED)

## GMP
###########################################################################

macro(FIND_GMP)
    set(ARGS ${ARGN})
    if(FETCH_GMP)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    elseif(${NO_SYSTEM_PATH})
        list(APPEND ARGS NO_CMAKE_SYSTEM_PATH)
    endif()
    find_package(gmp ${ARGS})
    find_package(gmpxx ${ARGS})
endmacro()
    
if(ENABLE_GMP)
    #message(STATUS "FETCH_GMP_IMPL=${FETCH_GMP_IMPL}")
    if (FETCH_GMP_IMPL)
        FIND_GMP(QUIET)
        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getGMP.cmake")
    endif()

    FIND_GMP(REQUIRED)
    endif()


#######################################
# libDivide

macro(FIND_LIBDIVIDE)
    set(ARGS ${ARGN})

    #explicitly asked to fetch libdivide
    if(FETCH_LIBDIVIDE)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${OC_THIRDPARTY_HINT})
    endif()

    find_path(LIBDIVIDE_INCLUDE_DIRS "libdivide.h" PATH_SUFFIXES "include" ${ARGS})
    if(EXISTS "${LIBDIVIDE_INCLUDE_DIRS}/libdivide.h")
        set(LIBDIVIDE_FOUND ON)
    else()
        set(LIBDIVIDE_FOUND OFF)
    endif()

endmacro()

if(FETCH_LIBDIVIDE_IMPL)
    FIND_LIBDIVIDE(QUIET)
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getLibDivide.cmake)
endif()

FIND_LIBDIVIDE(REQUIRED)

if(NOT TARGET libdivide)
    add_library(libdivide INTERFACE IMPORTED)

    target_include_directories(libdivide INTERFACE 
                    $<BUILD_INTERFACE:${LIBDIVIDE_INCLUDE_DIRS}>
                    $<INSTALL_INTERFACE:>)
endif()

message(STATUS "LIBDIVIDE_INCLUDE_DIRS:  ${LIBDIVIDE_INCLUDE_DIRS}")





# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
cmake_policy(POP)
