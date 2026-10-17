file(GLOB_RECURSE EURIBOR3M_CURVE_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(euribor3m_curve ${EURIBOR3M_CURVE_FILES})

target_link_libraries(euribor3m_curve dal_library)


if(MSVC)
else()
    target_link_libraries(euribor3m_curve pthread)
endif()

install(TARGETS euribor3m_curve
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
