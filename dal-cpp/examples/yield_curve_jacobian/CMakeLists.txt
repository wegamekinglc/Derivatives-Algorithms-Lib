file(GLOB_RECURSE YIELD_CURVE_JACOBIAN_FILES "*.hpp" "*.cpp")
add_executable(yield_curve_jacobian ${YIELD_CURVE_JACOBIAN_FILES})

target_link_libraries(yield_curve_jacobian dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(yield_curve_jacobian XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(yield_curve_jacobian CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(yield_curve_jacobian adept)
endif()

if(MSVC)
else()
    target_link_libraries(yield_curve_jacobian pthread)
endif()

install(TARGETS yield_curve_jacobian
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
