include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

add_executable(b_tree_tests b_tree_tests.cpp)

target_link_libraries(b_tree_tests PRIVATE gtest_main b_tree_lib)

target_include_directories(b_tree_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)