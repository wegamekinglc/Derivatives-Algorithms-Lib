#add_custom_command(OUTPUT ${QL_GENERATED_HEADERS}
#        COMMAND ${CMAKE_COMMAND}
#        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
#        -DBINARY_DIR=${CMAKE_BINARY_DIR}
#        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateHeaders.cmake
#        COMMENT "Generating headers..."
#        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

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