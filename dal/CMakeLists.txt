if (MSVC)
    add_definitions(-DIS_BASE)
endif()

file(GLOB_RECURSE DAL_FILES "*.hpp" "*.cpp")
list(FILTER DAL_FILES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/auto/.*")

add_library(dal_library ${DAL_FILES})

set_target_properties(dal_library PROPERTIES
        EXPORT_NAME ${PACKAGE_NAME}
        OUTPUT_NAME ${PACKAGE_NAME}
        VERSION ${DAL_VERSION})

install(DIRECTORY . DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_library EXPORT DALTargets
        ARCHIVE DESTINATION ${DAL_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${DAL_INSTALL_LIBDIR})