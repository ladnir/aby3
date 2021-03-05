
#project(aby)



	
#############################################
#              Build aby3                 #
#############################################

file(GLOB_RECURSE SRCS *.cpp)
 

add_library(aby3 ${SRCS})

target_include_directories(aby3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/linux/eigen")
target_include_directories(aby3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/linux/json/include")
target_include_directories(aby3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/linux/function2/include")

target_include_directories(aby3 PUBLIC ${Boost_INCLUDE_DIR}) 
target_include_directories(aby3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../) 
target_include_directories(aby3 PUBLIC ${libOTe_INC}) 

target_link_libraries(aby3 ${libOTe_LIB} ${cryptoTools_LIB} ${Boost_LIBRARIES})

