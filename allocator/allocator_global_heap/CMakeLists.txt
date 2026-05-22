cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(tests)

add_subdirectory(../../common ${CMAKE_CURRENT_BINARY_DIR}/common_build)
add_subdirectory(../allocator ${CMAKE_CURRENT_BINARY_DIR}/allocator_build)

add_library(
        sys_prog_allctr_allctr_glbl_hp
        src/allocator_global_heap.cpp)

target_include_directories(
        sys_prog_allctr_allctr_glbl_hp
        PUBLIC
        ./include)

target_link_libraries(
        sys_prog_allctr_allctr_glbl_hp
        PUBLIC
        sys_prog_cmmn)
target_link_libraries(
        sys_prog_allctr_allctr_glbl_hp
        PUBLIC
        sys_prog_allctr_allctr)