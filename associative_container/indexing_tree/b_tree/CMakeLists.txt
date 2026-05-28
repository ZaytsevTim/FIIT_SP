cmake_minimum_required(VERSION 3.15)
project(b_tree)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к зависимостям
set(ALLOCATOR_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../allocator/allocator/include)
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../allocator/common/include)
set(ASSOCIATIVE_CONTAINER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Основная библиотека
add_library(b_tree_lib INTERFACE)

target_include_directories(b_tree_lib INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ALLOCATOR_INCLUDE_DIR}
    ${COMMON_INCLUDE_DIR}
    ${ASSOCIATIVE_CONTAINER_DIR}/include
)

# Находим Boost
find_package(Boost REQUIRED)
target_link_libraries(b_tree_lib INTERFACE Boost::boost)

# Подключаем тесты
add_subdirectory(tests)