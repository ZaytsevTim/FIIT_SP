include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

add_executable(
    sys_prog_allctr_allctr_glbl_hp_tests
    allocator_global_heap_tests.cpp
)

target_link_libraries(
    sys_prog_allctr_allctr_glbl_hp_tests
    PRIVATE
    gtest_main
    sys_prog_allctr_allctr_glbl_hp
    sys_prog_cmmn
    sys_prog_allctr_allctr
)

add_executable(
    demo
    ../showcase/demo.cpp
)

target_link_libraries(
    demo
    PRIVATE
    sys_prog_allctr_allctr_glbl_hp
)