
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was Config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

####################################################################################


if(NOT MACORO_FIND_QUIETLY)
	message("macoroConfig.cmake: ${CMAKE_CURRENT_LIST_FILE}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/macoroTargets.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/macoroBuildOptions.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/macoroFindDeps.cmake")
