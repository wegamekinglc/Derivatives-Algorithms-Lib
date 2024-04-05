file(GLOB_RECURSE EXCEL_FILES "*.hpp" "*.cpp")
add_definitions(-DIS_BASE)
add_library(dal_excel SHARED ${EXCEL_FILES})
target_link_libraries(dal_excel dal_public)

if(DEFINED USE_XAD)
        target_link_libraries(dal_excel xad)
endif()

set_target_properties(dal_excel PROPERTIES SUFFIX .xll)

install(TARGETS dal_excel
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )