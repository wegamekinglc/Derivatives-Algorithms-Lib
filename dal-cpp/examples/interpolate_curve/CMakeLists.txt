file(GLOB_RECURSE INTERPOLATE_CURVE_FILES "*.hpp" "*.cpp")
add_executable(interpolate_curve ${INTERPOLATE_CURVE_FILES})

target_link_libraries(interpolate_curve dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(interpolate_curve XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(interpolate_curve CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(interpolate_curve adept)
endif()

if(MSVC)
else()
    target_link_libraries(interpolate_curve pthread)
endif()

install(TARGETS interpolate_curve
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
