
add_definitions(-DIS_BASE)
if (BUILD_SHARED_LIBS)
    file(GLOB_RECURSE PUBLIC_FILES "*.hpp" "*.cpp")
    add_library(dal_public ${PUBLIC_FILES})
    target_link_libraries(dal_public dal_library)
else()
    file(GLOB_RECURSE PUBLIC_FILES "*.hpp" "*.cpp" "${PROJECT_SOURCE_DIR}/dal/*.h" "${PROJECT_SOURCE_DIR}/dal/*.hpp" "${PROJECT_SOURCE_DIR}/dal/*.cpp")
    list(FILTER PUBLIC_FILES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/auto/.*")
    list(FILTER PUBLIC_FILES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/storage/_repository.*")
    add_library(dal_public ${PUBLIC_FILES})
endif()

if(DAL_HAS_EXCEL)
    target_compile_definitions(dal_public PRIVATE
        USE_EXCEL_REPORT
        OFFICE_MSO_PATH="${OFFICE_MSO_DLL}"
        OFFICE_VBE_PATH="${OFFICE_VBE_OLB}"
        OFFICE_EXCEL_PATH="${OFFICE_EXCEL_EXE}"
    )
endif()

if(DAL_USE_XAD_AAD)
    if (BUILD_SHARED_LIBS)
        target_link_libraries(dal_library XAD::xad)
    else()
        target_link_libraries(dal_public XAD::xad)
    endif()
elseif(DAL_USE_CODIPACK_AAD)
    if (BUILD_SHARED_LIBS)
        target_link_libraries(dal_library CoDiPack)
    else()
        target_link_libraries(dal_public CoDiPack)
    endif()
endif ()

install(DIRECTORY . DESTINATION include/public/src
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_public
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )