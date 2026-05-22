cmake_minimum_required(VERSION 3.10)
project(allocator_boundary_tags)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../common ${CMAKE_CURRENT_BINARY_DIR}/common_build)
add_subdirectory(../allocator ${CMAKE_CURRENT_BINARY_DIR}/allocator_build)

add_library(
    sys_prog_allctr_allctr_bndry_tgs
    src/allocator_boundary_tags.cpp
)

target_include_directories(
    sys_prog_allctr_allctr_bndry_tgs
    PUBLIC
    ./include
)

target_link_libraries(
    sys_prog_allctr_allctr_bndry_tgs
    PUBLIC
    sys_prog_cmmn
    sys_prog_allctr_allctr
)

add_subdirectory(tests)

add_executable(
    demo_boundary_tags
    showcase/demo.cpp
)

target_link_libraries(
    demo_boundary_tags
    PRIVATE
    sys_prog_allctr_allctr_bndry_tgs
)

target_include_directories(
    demo_boundary_tags
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../allocator/include
)