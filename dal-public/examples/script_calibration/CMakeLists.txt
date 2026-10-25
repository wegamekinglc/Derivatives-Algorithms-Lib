add_executable(script_calibration script_calibration.cpp)
target_link_libraries(script_calibration PRIVATE DAL::public)
install(TARGETS script_calibration RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
