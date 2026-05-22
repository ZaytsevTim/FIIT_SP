include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

add_executable(tests_buddies
    allocator_buddies_system_tests.cpp
)

target_link_libraries(tests_buddies
    PRIVATE
    gtest_main
    buddies
    sys_prog_cmmn
    sys_prog_allctr_allctr
)

target_include_directories(tests_buddies
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../allocator/include
)