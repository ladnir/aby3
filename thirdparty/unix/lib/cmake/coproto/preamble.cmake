    
if(NOT DEFINED COPROTO_CONFIG)
	if(MSVC)
		set(COPROTO_CONFIG_NAME "${CMAKE_BUILD_TYPE}")
		if("${COPROTO_CONFIG_NAME}" STREQUAL "RelWithDebInfo" OR "${COPROTO_CONFIG_NAME}" STREQUAL "")
			set(COPROTO_CONFIG_NAME "Release")
		endif()
		set(COPROTO_CONFIG "x64-${COPROTO_CONFIG_NAME}")
	elseif(APPLE)
		set(COPROTO_CONFIG "osx")
	else()
		set(COPROTO_CONFIG "linux")
	endif()
endif()

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in)
	set(COPROTO_IN_BUILD_TREE ON)
else()
	set(COPROTO_IN_BUILD_TREE OFF)
endif()

if(COPROTO_IN_BUILD_TREE)

    # we currenty are in the vole psi source tree, vole-psi/cmake
	if(NOT DEFINED COPROTO_BUILD_DIR)
		set(COPROTO_BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/../out/build/${COPROTO_CONFIG}")
		get_filename_component(COPROTO_BUILD_DIR ${COPROTO_BUILD_DIR} ABSOLUTE)
	endif()

	if(COPROTO_BUILD AND NOT (${CMAKE_BINARY_DIR} STREQUAL ${COPROTO_BUILD_DIR}))
		message(WARNING "incorrect build directory. \n\tCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}\nbut expect\n\tCOPROTO_BUILD_DIR=${COPROTO_BUILD_DIR}")
	endif()

	if(NOT DEFINED COPROTO_STAGE)
		set(COPROTO_STAGE "${CMAKE_CURRENT_LIST_DIR}/../out/install/${COPROTO_CONFIG}")
		get_filename_component(COPROTO_STAGE ${COPROTO_STAGE} ABSOLUTE)
	endif()

	
	if(NOT COPROTO_THIRDPARTY_CLONE_DIR)
		get_filename_component(COPROTO_THIRDPARTY_CLONE_DIR "${CMAKE_CURRENT_LIST_DIR}/../out/" ABSOLUTE)
	endif()
else()
    # we currenty are in install tree, <install-prefix>/lib/cmake/vole-psi
	if(NOT DEFINED COPROTO_STAGE)
		set(COPROTO_STAGE "${CMAKE_CURRENT_LIST_DIR}/../../..")
		get_filename_component(COPROTO_STAGE ${COPROTO_STAGE} ABSOLUTE)
	endif()
endif()

