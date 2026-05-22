include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

add_executable(
    sys_prog_allctr_allctr_srtd_lst_tests
    allocator_sorted_list_tests.cpp
)

target_link_libraries(
    sys_prog_allctr_allctr_srtd_lst_tests
    PRIVATE
    gtest_main
    sys_prog_allctr_allctr_srtd_lst
    sys_prog_cmmn
    sys_prog_allctr_allctr
)