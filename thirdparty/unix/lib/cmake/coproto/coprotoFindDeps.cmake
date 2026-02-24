cmake_policy(PUSH)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0045 NEW)
cmake_policy(SET CMP0074 NEW)



set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${COPROTO_STAGE};${CMAKE_PREFIX_PATH}")

## span-lite
###########################################################################
if(COPROTO_ENABLE_SPAN)

    macro(FIND_SPAN)
        set(ARGS ${ARGN})

        #explicitly asked to fetch span
        if(COPROTO_FETCH_SPAN)
            list(APPEND ARGS NO_DEFAULT_PATH PATHS ${COPROTO_STAGE})
        elseif(${COPROTO_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
        endif()
    
        find_package(span-lite ${ARGS})

    endmacro()

    if((COPROTO_FETCH_AUTO OR COPROTO_FETCH_SPAN) AND COPROTO_BUILD)
        if(NOT COPROTO_FETCH_SPAN)
            FIND_SPAN(QUIET)
        endif()
        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getSpanLite.cmake")
    endif()
    FIND_SPAN(REQUIRED)
  
endif()


## macoro
###########################################################################

macro(FIND_MACORO)
    set(ARGS ${ARGN})

    #explicitly asked to fetch MACORO
    if(COPROTO_FETCH_MACORO)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${COPROTO_STAGE})
    elseif(${COPROTO_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    
    
    if(NOT DEFINED COPROTO_CPP_VER)
        message(FATAL_ERROR "COPROTO_CPP_VER not defined")
    endif()

    set(macoro_options cpp_${COPROTO_CPP_VER})
    if(COPROTO_PIC)
        set(macoro_options ${macoro_options} pic)
    else()
        set(macoro_options ${macoro_options} no_pic)
    endif()
    if(COPROTO_ASAN)
        set(macoro_options ${macoro_options} asan)
    else()
        set(macoro_options ${macoro_options} no_asan)
    endif()


    if(    "${COPROTO_BUILD_TYPE}" STREQUAL "Release"
        OR "${COPROTO_BUILD_TYPE}" STREQUAL "Debug"
        OR "${COPROTO_BUILD_TYPE}" STREQUAL "RelWithDebInfo" )
        set(macoro_options ${macoro_options} ${COPROTO_BUILD_TYPE} )
    endif()

    find_package(macoro ${ARGS} COMPONENTS ${macoro_options})

endmacro()

if((COPROTO_FETCH_AUTO OR COPROTO_FETCH_MACORO) AND COPROTO_BUILD)
    if(NOT COPROTO_FETCH_MACORO)
        FIND_MACORO(QUIET)
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getMacoro.cmake")
endif()
FIND_MACORO(REQUIRED)


## function2
###########################################################################

macro(FIND_FUNCTION2)
    set(ARGS ${ARGN})

    #explicitly asked to fetch function2
    if(COPROTO_FETCH_FUNCTION2)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${COPROTO_STAGE})
    elseif(${COPROTO_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    
    find_package(function2 ${ARGS})

endmacro()

if((COPROTO_FETCH_AUTO OR COPROTO_FETCH_FUNCTION2) AND COPROTO_BUILD)
    if(NOT COPROTO_FETCH_FUNCTION2)
        FIND_FUNCTION2(QUIET)
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getFunction2.cmake")
endif()
FIND_FUNCTION2(REQUIRED)
set_target_properties(function2::function2 PROPERTIES INTERFACE_COMPILE_FEATURES "")

## Boost
###########################################################################


if(COPROTO_ENABLE_BOOST)

    macro(FIND_BOOST)
        set(ARGS ${ARGN})
        if(COPROTO_FETCH_BOOST)
            list(APPEND ARGS NO_DEFAULT_PATH  PATHS ${COPROTO_STAGE} )
        elseif(${COPROTO_NO_SYSTEM_PATH})
            list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
        endif()

        option(Boost_USE_MULTITHREADED "mt boost" ON)
        option(Boost_USE_STATIC_LIBS "static boost" ON)


        if(MSVC)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                option(Boost_USE_DEBUG_RUNTIME "boost debug runtime" ON)
            else()
                option(Boost_USE_DEBUG_RUNTIME "boost debug runtime" OFF)
            endif()
            option(Boost_LIB_PREFIX "Boost_LIB_PREFIX" "lib")
        else()
            option(Boost_USE_DEBUG_RUNTIME "boost debug runtime" OFF)
        endif()
        #set(Boost_DEBUG ON)  #<---------- Real life saver

        find_package(Boost 1.84.0 COMPONENTS system thread regex ${ARGS} ${COPROTO_FIND_PACKAGE_OPTIONS})
    endmacro()

    
    if((COPROTO_FETCH_AUTO OR COPROTO_FETCH_BOOST) AND COPROTO_BUILD)
        if(NOT COPROTO_FETCH_BOOST)
            FIND_BOOST(QUIET)
        endif()
        
        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getBoost.cmake")

    endif()


    FIND_BOOST(REQUIRED)

    if(NOT Boost_FOUND)
        message(FATAL_ERROR "Failed to find boost 1.84. When building coproto, add -DCOPROTO_FETCH_BOOST=ON or -DCOPROTO_FETCH_AUTO=ON to auto download.")
    endif()

    message(STATUS "\n\nBoost_LIB: ${Boost_LIBRARIES}" )
    message(STATUS "Boost_INC: ${Boost_INCLUDE_DIR}\n\n" )
endif()


## OpenSSL
###########################################################################

if(COPROTO_ENABLE_OPENSSL)
    find_package(OpenSSL REQUIRED)
    
    message("OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}")
    message("OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}")
    message("OPENSSL_LIBRARIES  =${OPENSSL_LIBRARIES}")
    message("OPENSSL_VERSION    =${OPENSSL_VERSION}")
    
endif()



# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
cmake_policy(POP)

find_package(Threads REQUIRED)