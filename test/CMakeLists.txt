add_executable(test_hwinfo test.cpp)
target_include_directories(test_hwinfo PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_hwinfo PRIVATE Catch2::Catch2WithMain fmt::fmt)

add_test(test_hwinfo test_hwinfo)