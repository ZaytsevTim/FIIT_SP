cmake_minimum_required(VERSION 3.10)
project(allocator_sorted_list)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../common ${CMAKE_CURRENT_BINARY_DIR}/common_build)
add_subdirectory(../allocator ${CMAKE_CURRENT_BINARY_DIR}/allocator_build)

add_subdirectory(tests)

add_library(
        sys_prog_allctr_allctr_srtd_lst
        src/allocator_sorted_list.cpp)

target_include_directories(
        sys_prog_allctr_allctr_srtd_lst
        PUBLIC
        ./include)

target_link_libraries(
        sys_prog_allctr_allctr_srtd_lst
        PUBLIC
        sys_prog_cmmn)
target_link_libraries(
        sys_prog_allctr_allctr_srtd_lst
        PUBLIC
        sys_prog_allctr_allctr)

add_executable(
    demo
    showcase/demo.cpp
)

target_link_libraries(
    demo
    PRIVATE
    sys_prog_allctr_allctr_srtd_lst
)