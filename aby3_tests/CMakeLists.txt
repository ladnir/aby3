
#project(aby3_Tests)

file(GLOB_RECURSE SRCS *.cpp)
add_library(aby3_Tests STATIC ${SRCS})

target_include_directories(aby3_Tests PUBLIC ${CMAKE_SOURCE_DIR}) 
target_link_libraries(aby3_Tests aby3)
