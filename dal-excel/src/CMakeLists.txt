file(GLOB_RECURSE EXCEL_FILES "*.hpp" "*.cpp")
add_definitions(-DIS_BASE)
add_library(dal_excel SHARED ${EXCEL_FILES})
target_link_libraries(dal_excel dal_public)
target_compile_definitions(dal_excel PRIVATE
    USE_EXCEL_REPORT
    OFFICE_MSO_PATH="${OFFICE_MSO_DLL}"
    OFFICE_VBE_PATH="${OFFICE_VBE_OLB}"
    OFFICE_EXCEL_PATH="${OFFICE_EXCEL_EXE}"
)

set_target_properties(dal_excel PROPERTIES SUFFIX .xll)

install(TARGETS dal_excel
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )