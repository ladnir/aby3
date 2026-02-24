
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was Config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

####################################################################################


include("${CMAKE_CURRENT_LIST_DIR}/bitpolymulTargets.cmake")


if(NOT bitpolymul_FIND_QUIETLY)
	message(STATUS "bitpolymulConfig.cmake : ${CMAKE_CURRENT_LIST_DIR}")
endif()
