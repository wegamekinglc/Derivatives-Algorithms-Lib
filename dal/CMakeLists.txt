if (MSVC)
    add_definitions(-DIS_BASE)
endif()

file(GLOB_RECURSE DAL_FILES "*.h" "*.hpp" "*.cpp" ".inc")
list(FILTER DAL_FILES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/auto/.*")
list(FILTER DAL_FILES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/storage/_repository.*")

add_library(dal_library ${DAL_FILES})

if(DAL_HAS_EXCEL)
    target_compile_definitions(dal_library PRIVATE
        USE_EXCEL_REPORT
        OFFICE_MSO_PATH="${OFFICE_MSO_DLL}"
        OFFICE_VBE_PATH="${OFFICE_VBE_OLB}"
        OFFICE_EXCEL_PATH="${OFFICE_EXCEL_EXE}"
    )
endif()

if (DAL_USE_XAD_AAD)
    target_link_libraries(dal_library XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(dal_library CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(dal_library adept)
endif()

set_target_properties(dal_library PROPERTIES
        EXPORT_NAME ${PACKAGE_NAME}
        OUTPUT_NAME ${PACKAGE_NAME}
        VERSION ${DAL_VERSION})

install(DIRECTORY . DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_library EXPORT DALTargets
        ARCHIVE DESTINATION ${DAL_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${DAL_INSTALL_LIBDIR})
