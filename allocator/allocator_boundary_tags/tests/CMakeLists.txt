include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

add_executable(
    tests_boundary_tags
    allocator_boundary_tags_tests.cpp
)

target_link_libraries(
    tests_boundary_tags
    PRIVATE
    gtest_main
    sys_prog_allctr_allctr_bndry_tgs
    sys_prog_cmmn
    sys_prog_allctr_allctr
)