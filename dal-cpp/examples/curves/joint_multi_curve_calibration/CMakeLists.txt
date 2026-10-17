file(GLOB_RECURSE JOINT_MULTI_CURVE_CALIBRATION_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(joint_multi_curve_calibration ${JOINT_MULTI_CURVE_CALIBRATION_FILES})

target_link_libraries(joint_multi_curve_calibration dal_library)


if(MSVC)
else()
    target_link_libraries(joint_multi_curve_calibration pthread)
endif()

install(TARGETS joint_multi_curve_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
