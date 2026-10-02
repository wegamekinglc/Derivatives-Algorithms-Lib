add_executable(gsr_slv_calibration gsr_slv_calibration.cpp)
target_link_libraries(gsr_slv_calibration PRIVATE DAL::public)

install(TARGETS gsr_slv_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
