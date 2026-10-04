file(GLOB_RECURSE XCCY_CURVE_CALIBRATION_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(xccy_curve_calibration ${XCCY_CURVE_CALIBRATION_FILES})

target_link_libraries(xccy_curve_calibration dal_library)


if(MSVC)
else()
    target_link_libraries(xccy_curve_calibration pthread)
endif()

install(TARGETS xccy_curve_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
