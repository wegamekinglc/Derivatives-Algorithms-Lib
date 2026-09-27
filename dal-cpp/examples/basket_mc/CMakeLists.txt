add_executable(basket_mc basket_mc.cpp)
target_link_libraries(basket_mc dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(basket_mc XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(basket_mc CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(basket_mc adept)
endif()

if(NOT MSVC)
    target_link_libraries(basket_mc pthread)
endif()

install(TARGETS basket_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
