cmake_minimum_required (VERSION 2.8.11)


set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)

set(CMAKE_C_FLAGS "-Wall  -maes -msse2 -msse4.1 -msse3 -mavx -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-ignored-attributes -Wno-int-in-bool-context")
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
SET(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG") 
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")


#############################################
#       Library/includes locations          #
#############################################

# default location for libOTe
set(libOTe_Dirs "${CMAKE_SOURCE_DIR}/../libOTe/")

# default location for miracl
set(Miracl_Dirs "${libOTe_Dirs}/cryptoTools/thirdparty/linux/miracl/")

# default location for Boost
set(BOOST_ROOT "${libOTe_Dirs}/cryptoTools/thirdparty/linux/boost/")


#############################################
#        Build aby3, com-psi, tests         #
#############################################
add_subdirectory(aby3)
add_subdirectory(aby3-DB)

add_subdirectory(aby3-ML)
add_subdirectory(aby3-DB_tests)
add_subdirectory(aby3_tests)

add_subdirectory(frontend)


