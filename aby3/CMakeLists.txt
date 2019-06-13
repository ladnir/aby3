
#project(aby)


#############################################
#                 Link Boost                #
#############################################


set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     ON)

find_package(Boost  COMPONENTS system thread)

if(Boost_FOUND)
  message( "Found Boost at ${Boost_LIBRARIES}")
else()
   message(FATAL_ERROR "Failed to find boost at " ${Boost_Lib_Dirs} " Need system thread")
endif()

 
#############################################
#                 Link Miracl               #
#############################################
find_library(
  MIRACL_LIB 
  NAMES miracl  
  HINTS "${Miracl_Dirs}/miracl/source/")
  
# if we cant fint it, throw an error
if(NOT MIRACL_LIB)
  Message(${MIRACL_LIB})
  message(FATAL_ERROR "Failed to find miracl at " ${Miracl_Dirs})
else()
  message("Miracl at  ${MIRACL_LIB}")
endif()



#############################################
#                 Link libOTe               #
#############################################


find_library(
    libOTe_LIB
    NAMES libOTe
    HINTS "${libOTe_Dirs}/lib/")
	
find_library(
    SimplestOT_LIB
    NAMES SimplestOT
    HINTS "${libOTe_Dirs}/lib/")

find_library(
    cryptoTools_LIB
    NAMES cryptoTools
    HINTS "${libOTe_Dirs}/lib/")

	
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
target_include_directories(aby3 PUBLIC ${libOTe_Dirs}) 
target_include_directories(aby3 PUBLIC ${libOTe_Dirs}/cryptoTools) 

target_link_libraries(aby3 ${libOTe_LIB} ${SimplestOT_LIB} ${cryptoTools_LIB} ${MIRACL_LIB}  ${Boost_LIBRARIES})

