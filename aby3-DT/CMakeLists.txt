
#project(com-psi)


file(GLOB_RECURSE SRCS *.cpp)
 

add_library(aby3-DT ${SRCS})


target_link_libraries(aby3-DT aby3 com-psi)

