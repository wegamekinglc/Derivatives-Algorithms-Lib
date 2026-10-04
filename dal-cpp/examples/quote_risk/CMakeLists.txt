file(GLOB_RECURSE QUOTE_RISK_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(quote_risk ${QUOTE_RISK_FILES})

target_link_libraries(quote_risk dal_library)


if(NOT MSVC)
    target_link_libraries(quote_risk pthread)
endif()

install(TARGETS quote_risk
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
