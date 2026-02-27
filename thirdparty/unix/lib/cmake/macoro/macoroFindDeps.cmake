cmake_policy(PUSH)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0045 NEW)
cmake_policy(SET CMP0074 NEW)

include("${CMAKE_CURRENT_LIST_DIR}/macoroPreamble.cmake")


set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${MACORO_STAGE};${CMAKE_PREFIX_PATH}")

## optional-lite 
###########################################################################
macro(FIND_OPTIONAL)
    set(ARGS ${ARGN})

    #explicitly asked to fetch optional
    if(MACORO_FETCH_OPTIONAL)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${MACORO_STAGE})
    elseif(${MACORO_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    
    find_package(optional-lite ${ARGS})

endmacro()

if(MACORO_OPTIONAL_LITE_V)
    if((MACORO_FETCH_AUTO OR MACORO_FETCH_OPTIONAL) AND MACORO_BUILD)
        if(NOT MACORO_FETCH_OPTIONAL)
            FIND_OPTIONAL(QUIET)
        endif()
        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getOptionalLite.cmake")
    endif()

    FIND_OPTIONAL(REQUIRED)
endif()

## variant-lite
###########################################################################

macro(FIND_VARIANT)
    set(ARGS ${ARGN})

    #explicitly asked to fetch variant
    if(MACORO_FETCH_VARIANT)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${MACORO_STAGE})
    elseif(${MACORO_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    
    find_package(variant-lite ${ARGS})

endmacro()

if(MACORO_VARIANT_LITE_V)
    if((MACORO_FETCH_AUTO OR MACORO_FETCH_OPTIONAL) AND MACORO_BUILD)
        if(NOT MACORO_FETCH_VARIANT)
            FIND_VARIANT(QUIET)
        endif()

        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getVariantLite.cmake")
    endif()

    FIND_VARIANT(REQUIRED)
endif()



###########################################################################

# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
cmake_policy(POP)

find_package(Threads REQUIRED)