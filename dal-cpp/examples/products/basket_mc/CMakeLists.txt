add_executable(basket_mc basket_mc.cpp)
target_link_libraries(basket_mc dal_library)


if(NOT MSVC)
    target_link_libraries(basket_mc pthread)
endif()

install(TARGETS basket_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
