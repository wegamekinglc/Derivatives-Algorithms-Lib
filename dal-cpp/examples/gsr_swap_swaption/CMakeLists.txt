add_executable(gsr_swap_swaption gsr_swap_swaption.cpp)
target_link_libraries(gsr_swap_swaption PRIVATE DAL::cpp)

install(TARGETS gsr_swap_swaption
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
