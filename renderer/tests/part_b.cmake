# Optional integration by A: include(tests/part_b.cmake) at the end of renderer/CMakeLists.txt.
add_executable(phase_lighting_tests tests/phase_lighting_tests.cpp src/pathtracer/sampler.cpp)
target_link_libraries(phase_lighting_tests PRIVATE pt31)
enable_testing()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/part_b_results")
add_test(NAME phase_lighting_tests COMMAND phase_lighting_tests "${CMAKE_CURRENT_BINARY_DIR}/part_b_results")
