file(GLOB_RECURSE SOBOL_FILES "*.hpp" "*.cpp")
add_executable(sobol ${SOBOL_FILES})

target_link_libraries(sobol ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(sobol pthread)
endif()

install(TARGETS sobol
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )