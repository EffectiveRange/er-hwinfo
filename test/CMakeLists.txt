add_executable(test_hwinfo test.cpp)
target_link_libraries(test_hwinfo PRIVATE lib-er-hwinfo Catch2::Catch2WithMain)

add_test(test_hwinfo test_hwinfo)