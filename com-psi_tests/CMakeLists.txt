
#project(com-psi_Tests)

file(GLOB_RECURSE SRCS *.cpp)
add_library(com-psi_Tests STATIC ${SRCS})

target_include_directories(com-psi_Tests PUBLIC ${CMAKE_SOURCE_DIR}) 
target_link_libraries(com-psi_Tests com-psi)
