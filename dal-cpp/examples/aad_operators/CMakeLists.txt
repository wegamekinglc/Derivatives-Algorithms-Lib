add_executable(aad_operators aad_operators.cpp)
target_link_libraries(aad_operators PRIVATE DAL::cpp)
install(TARGETS aad_operators RUNTIME DESTINATION bin)
