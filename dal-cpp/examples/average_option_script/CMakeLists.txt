add_executable(average_option_script average_option_script.cpp)
target_link_libraries(average_option_script dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(average_option_script XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(average_option_script CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(average_option_script adept)
endif()

if(NOT MSVC)
    target_link_libraries(average_option_script pthread)
endif()

install(TARGETS average_option_script
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
