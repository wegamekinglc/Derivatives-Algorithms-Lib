file(GLOB_RECURSE XCCY_MTM_CALIBRATION_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(xccy_mtm_calibration ${XCCY_MTM_CALIBRATION_FILES})

target_link_libraries(xccy_mtm_calibration dal_library)


if(NOT MSVC)
    target_link_libraries(xccy_mtm_calibration pthread)
endif()

install(TARGETS xccy_mtm_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
