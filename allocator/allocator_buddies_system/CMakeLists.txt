cmake_minimum_required(VERSION 3.10)
project(allocator_buddies_system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../common ${CMAKE_CURRENT_BINARY_DIR}/common_build)
add_subdirectory(../allocator ${CMAKE_CURRENT_BINARY_DIR}/allocator_build)

add_library(buddies
    src/allocator_buddies_system.cpp
)

target_include_directories(buddies
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../allocator/include
)

target_link_libraries(buddies
    PUBLIC
    sys_prog_cmmn
    sys_prog_allctr_allctr
)

add_subdirectory(tests)

add_executable(demo_buddies
    showcase/demo.cpp
)

target_link_libraries(demo_buddies
    PRIVATE
    buddies
)

target_include_directories(demo_buddies
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../allocator/include
)