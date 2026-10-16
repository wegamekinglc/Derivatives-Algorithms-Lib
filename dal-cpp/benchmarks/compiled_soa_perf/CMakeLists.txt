add_executable(compiled_soa_perf compiled_soa_perf.cpp)
target_link_libraries(compiled_soa_perf PRIVATE DAL::cpp)
add_test(NAME compiled_soa_perf COMMAND compiled_soa_perf weekly 32 65 1 steady)
set_tests_properties(compiled_soa_perf PROPERTIES LABELS "benchmark")
