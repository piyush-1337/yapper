find_package(Catch2 3 REQUIRED)
add_executable(yapper_tests ring_buffer.cpp)

target_link_libraries(yapper_tests PRIVATE yapper Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(yapper_tests)
