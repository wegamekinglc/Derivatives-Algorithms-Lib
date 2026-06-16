file(GLOB_RECURSE EURIBOR3M_CURVE_FILES "*.hpp" "*.cpp")
add_executable(euribor3m_curve ${EURIBOR3M_CURVE_FILES})

target_link_libraries(euribor3m_curve dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(euribor3m_curve XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(euribor3m_curve CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(euribor3m_curve adept)
endif()

if(MSVC)
else()
    target_link_libraries(euribor3m_curve pthread)
endif()

install(TARGETS euribor3m_curve
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
