cmake_minimum_required (VERSION 3.15)

project(aby3 VERSION 1.0.0)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")


	if(MSVC)

	else()

	
		set(CMAKE_C_FLAGS "-Wall  -maes -msse2 -msse4.1 -msse3 -mavx -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-ignored-attributes -Wno-int-in-bool-context")
		set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14")

	
		# Select flags.
		SET(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG") 
		SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
		SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")

	endif()

	# Set a default build type for single-configuration
	# CMake generators if no build type is set.
	if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	   SET(CMAKE_BUILD_TYPE Release)
	endif()

	if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	  message(WARNING "\nCo5nfiguration NOT set to Release. ")

		if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		  #message(WARNING "Debug build.")
		elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release") 
		  #message(WARNING "Release build.")
		elseif("${CMAKE_BUILD_TYPE}" STREQUAL  "RelWithDebInfo")
		  #message(WARNING "RelWithDebInfo=Release with debug info build.")
		else()
		  message(ERROR ": Unknown build type - ${CMAKE_BUILD_TYPE}  Please use one of Debug, Release, or RelWithDebInfo\n\n" )
		endif()
	endif()
endif()

#############################################
#       Library/includes locations          #
#############################################

if(MSVC)
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/thirdparty/win-debug/")
	endif()
	list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/thirdparty/win/")
else()
	list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/thirdparty/unix/")
endif()

find_package(libOTe HINTS "${libOTe_Dirs}")

if(NOT ENABLE_BOOST)
	message(FATAL_ERROR "libOTe/cryptoTools needs to have boost enabled.")
endif()

if(NOT ENABLE_CIRCUITS)
	message(FATAL_ERROR "libOTe/cryptoTools needs to have ENABLE_CIRCUITS defined.")
endif()

#############################################
#        Build aby3, com-psi, tests         #
#############################################
add_subdirectory(aby3)
add_subdirectory(aby3-DB)

add_subdirectory(aby3-ML)
add_subdirectory(aby3-DB_tests)

add_subdirectory(aby3_tests)


add_subdirectory(frontend)


