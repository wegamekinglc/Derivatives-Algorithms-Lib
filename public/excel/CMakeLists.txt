file(GLOB_RECURSE EXCEL_FILES "../src/*.hpp" "../src/*.cpp" "*.hpp" "*.cpp")
add_definitions(-DIS_BASE)
add_library(dal_excel SHARED ${EXCEL_FILES})
set_target_properties(dal_excel PROPERTIES SUFFIX .xll)
target_link_libraries(dal_excel dal_library)

install(TARGETS dal_excel
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )