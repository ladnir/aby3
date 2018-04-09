cmake_minimum_required (VERSION 2.8.11)


set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)

set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mpclmul -Wfatal-errors -pthread -fPIC -Wno-ignored-attributes ")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14")

# Set a default build type for single-configuration
# CMake generators if no build type is set.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE Release)
endif()

if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
  message(WARNING "\nConfiguration NOT set to Release. Please call cmake with -DCMAKE_BUILD_TYPE=Release  instead of ${CMAKE_BUILD_TYPE}")

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	  #message(WARNING "Debug build.")
	elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release") 
	  #message(WARNING "Release build.")
	elseif("${CMAKE_BUILD_TYPE}" STREQUAL  "RELWITHDEBINFO")
	  #message(WARNING "RELWITHDEBINFO=Release with debug info build.")
	else()
	  message(ERROR ": Unknown build type - ${CMAKE_BUILD_TYPE}  Please use one of Debug, Release, or RELWITHDEBINFO\n\n" )
	endif()
endif()

# Select flags.
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG") 
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")


# add the source files 
file(GLOB_RECURSE SRCS SecureML/*.cpp)

SET(libOTe_dir ${CMAKE_SOURCE_DIR}/../libOTe)

include_directories("${CMAKE_SOURCE_DIR}/SecureML")
include_directories("${CMAKE_SOURCE_DIR}/thirdparty/linux/eigen")
include_directories(${libOTe_dir})
include_directories(${libOTe_dir}/cryptoTools)

link_directories(
	"${libOTe_dir}lib" 
	"${libOTe_dir}cryptoTools/thirdparty/linux/miracl/miracl/source/")

add_executable(secureML  ${SRCS})
target_link_libraries(secureML libOTe cryptoTools miracl)


## Boost
###########################################################################

set(BOOST_ROOT "${libOTe_dir}/cryptoTools/thirdparty/linux/boost/")

set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     ON)

find_package(Boost COMPONENTS system thread)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Failed to find boost at `" ${BOOST_ROOT} "`. Need system thread")
endif()

include_directories(${Boost_INCLUDE_DIR}) 
target_link_libraries(secureML ${Boost_LIBRARIES})

