enable_testing()

find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(exys_unit_test main.cc test_parser.cc)

target_link_libraries(exys_unit_test exys ${GTEST_BOTH_LIBRARIES} )
