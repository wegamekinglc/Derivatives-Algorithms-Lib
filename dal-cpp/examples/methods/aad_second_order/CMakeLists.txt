add_executable(aad_second_order aad_second_order.cpp)
target_link_libraries(aad_second_order PRIVATE DAL::cpp)
install(TARGETS aad_second_order RUNTIME DESTINATION bin)
