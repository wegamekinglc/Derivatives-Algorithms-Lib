add_executable(gsr_swap_swaption gsr_swap_swaption.cpp)
target_link_libraries(gsr_swap_swaption PRIVATE DAL::public)
dal_apply_platform_options(gsr_swap_swaption)
add_test(NAME gsr_swap_swaption COMMAND gsr_swap_swaption)
set_tests_properties(gsr_swap_swaption PROPERTIES LABELS examples)

install(TARGETS gsr_swap_swaption
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
