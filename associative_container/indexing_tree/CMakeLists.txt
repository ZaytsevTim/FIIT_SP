cmake_minimum_required(VERSION 3.15)
project(b_tree)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Пути к зависимостям (аллокаторы и общие файлы)
add_subdirectory(../../../allocator/common ${CMAKE_CURRENT_BINARY_DIR}/common_build)
add_subdirectory(../../../allocator/allocator ${CMAKE_CURRENT_BINARY_DIR}/allocator_build)

# Находим Boost
find_package(Boost REQUIRED)

# Основная библиотека
add_library(b_tree_lib INTERFACE)
target_include_directories(b_tree_lib INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../allocator/allocator/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../allocator/common/include
)
target_link_libraries(b_tree_lib INTERFACE 
    Boost::boost
    sys_prog_cmmn
    sys_prog_allctr_allctr
)

# Тесты
add_subdirectory(tests)