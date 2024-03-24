file(GLOB_RECURSE MATRIX_PERF_FILES "*.hpp" "*.cpp")

find_package(OpenMP REQUIRED)

add_executable(matrix_perf ${MATRIX_PERF_FILES})

target_link_libraries(matrix_perf dal_library OpenMP::OpenMP_CXX)

if(MSVC)
else()
    target_link_libraries(matrix_perf pthread)
endif()

install(TARGETS matrix_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )