

add_executable(test_hwinfo test.cpp)
target_link_libraries(test_hwinfo PRIVATE Catch2::Catch2WithMain fmt::fmt)

add_test(test_hwinfo test_hwinfo)