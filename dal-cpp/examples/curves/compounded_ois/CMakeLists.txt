add_executable(compounded_ois compounded_ois.cpp)
target_link_libraries(compounded_ois PRIVATE DAL::cpp)
install(TARGETS compounded_ois RUNTIME DESTINATION bin)
