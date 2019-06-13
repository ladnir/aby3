
#project(com-psi)


file(GLOB_RECURSE SRCS *.cpp)
 

add_library(com-psi ${SRCS})


target_link_libraries(com-psi aby3)

