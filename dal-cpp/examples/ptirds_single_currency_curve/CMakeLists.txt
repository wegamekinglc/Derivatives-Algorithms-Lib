file(GLOB_RECURSE PTIRDS_SINGLE_CURRENCY_CURVE_FILES "*.hpp" "*.cpp")
add_executable(ptirds_single_currency_curve ${PTIRDS_SINGLE_CURRENCY_CURVE_FILES})

target_link_libraries(ptirds_single_currency_curve dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(ptirds_single_currency_curve XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(ptirds_single_currency_curve CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(ptirds_single_currency_curve adept)
endif()

if(MSVC)
else()
    target_link_libraries(ptirds_single_currency_curve pthread)
endif()

install(TARGETS ptirds_single_currency_curve
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
