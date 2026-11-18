add_executable(json_perf json_perf.cpp)
target_link_libraries(json_perf PRIVATE DAL::cpp)
if(NOT MSVC)
    target_link_libraries(json_perf PRIVATE pthread)
endif()

install(TARGETS json_perf RUNTIME DESTINATION bin)
add_test(NAME json_perf COMMAND json_perf)
set_tests_properties(json_perf PROPERTIES LABELS "benchmark")
