add_executable(nanoql_multi_curve nanoql_multi_curve.cpp)
target_link_libraries(nanoql_multi_curve PRIVATE DAL::cpp)

install(TARGETS nanoql_multi_curve RUNTIME DESTINATION bin)
