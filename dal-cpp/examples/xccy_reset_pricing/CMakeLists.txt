file(GLOB_RECURSE XCCY_RESET_PRICING_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(xccy_reset_pricing ${XCCY_RESET_PRICING_FILES})
target_link_libraries(xccy_reset_pricing dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(xccy_reset_pricing XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(xccy_reset_pricing CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(xccy_reset_pricing adept)
endif()

if(NOT MSVC)
    target_link_libraries(xccy_reset_pricing pthread)
endif()

install(TARGETS xccy_reset_pricing
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
