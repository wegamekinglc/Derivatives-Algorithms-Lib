file(GLOB_RECURSE YIELD_CURVE_JACOBIAN_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(yield_curve_jacobian ${YIELD_CURVE_JACOBIAN_FILES})

target_link_libraries(yield_curve_jacobian dal_library)


if(MSVC)
else()
    target_link_libraries(yield_curve_jacobian pthread)
endif()

install(TARGETS yield_curve_jacobian
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
