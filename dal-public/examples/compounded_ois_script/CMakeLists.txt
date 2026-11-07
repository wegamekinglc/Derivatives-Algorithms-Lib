add_executable(compounded_ois_script compounded_ois_script.cpp)
target_link_libraries(compounded_ois_script PRIVATE DAL::public)
install(TARGETS compounded_ois_script RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
