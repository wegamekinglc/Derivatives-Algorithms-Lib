file(GLOB_RECURSE CURVE_CALIBRATION_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(curve_calibration ${CURVE_CALIBRATION_FILES})

target_link_libraries(curve_calibration dal_library)


if(MSVC)
else()
    target_link_libraries(curve_calibration pthread)
endif()

install(TARGETS curve_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
