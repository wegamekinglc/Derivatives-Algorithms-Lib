add_executable(gsr_script_calibration gsr_script_calibration.cpp)
target_link_libraries(gsr_script_calibration PRIVATE DAL::public)
install(TARGETS gsr_script_calibration RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
