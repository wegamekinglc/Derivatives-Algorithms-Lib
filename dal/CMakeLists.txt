get_dal_library_name(DAL_OUTPUT_NAME)


file(GLOB_RECURSE DAL_FILES "*.hpp" "*.cpp")

if(WIN32)
    if (${BUILD_SHARED_LIBS})
        message(FATAL_ERROR "You should build in STATIC mode as nothing is exported from the library. Use BUILD_SHARED_LIBS=OFF")
    endif()
    add_library(${DAL_OUTPUT_NAME} ${DAL_FILES})
    set_property(TARGET ${DAL_OUTPUT_NAME} PROPERTY PROJECT_LABEL "dal")
else()
    add_library(${DAL_OUTPUT_NAME} ${DAL_FILES})
endif()
set(DAL_LINK_LIBRARY ${DAL_OUTPUT_NAME} PARENT_SCOPE)


install(DIRECTORY . DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS ${DAL_OUTPUT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )