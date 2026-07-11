file(GLOB_RECURSE EXCELWRITER_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(excelwriter ${EXCELWRITER_FILES})

target_link_libraries(excelwriter dal_library)
target_compile_definitions(excelwriter PRIVATE
    USE_EXCEL_REPORT
    OFFICE_MSO_PATH="${OFFICE_MSO_DLL}"
    OFFICE_VBE_PATH="${OFFICE_VBE_OLB}"
    OFFICE_EXCEL_PATH="${OFFICE_EXCEL_EXE}"
)

if(DAL_USE_XAD_AAD)
    target_link_libraries(excelwriter XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(excelwriter CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(excelwriter adept)
endif ()


if(MSVC)
else()
    target_link_libraries(excelwriter pthread)
endif()

install(TARGETS excelwriter
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
