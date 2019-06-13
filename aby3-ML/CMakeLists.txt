
#project(aby3-ML)


file(GLOB_RECURSE SRCS *.cpp)
 

add_library(aby3-ML ${SRCS})


target_link_libraries(aby3-ML aby3)

