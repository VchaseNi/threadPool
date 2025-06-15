cmake_minimum_required(VERSION 3.5)
project(usecases)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
option(ENABLE_PRIORITY_QUEUE "Enable priority queue" OFF)
if (ENABLE_PRIORITY_QUEUE)
    add_compile_definitions(PRIORITY_QUEUE)
endif()

# include 
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# src
file(GLOB_RECURSE src ${CMAKE_CURRENT_SOURCE_DIR}/usecases.cpp)
set(SUB_DIR_LIB_SRCS
    ${src}
)
add_executable(${PROJECT_NAME} ${SUB_DIR_LIB_SRCS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} pthread)