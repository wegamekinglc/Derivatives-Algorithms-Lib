file(GLOB_RECURSE CURVE_CALIBRATION_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(curve_calibration_perf ${CURVE_CALIBRATION_PERF_FILES})

target_link_libraries(curve_calibration_perf dal_library)
target_include_directories(curve_calibration_perf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../externals/rapidjson/include")


if(MSVC)
else()
    target_link_libraries(curve_calibration_perf pthread)
endif()

install(TARGETS curve_calibration_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME curve_calibration_perf COMMAND curve_calibration_perf)
set_tests_properties(curve_calibration_perf PROPERTIES LABELS "benchmark")

add_test(NAME compounded_ois_perf_contract COMMAND curve_calibration_perf --compounded-ois)
set_tests_properties(compounded_ois_perf_contract PROPERTIES
    LABELS "benchmark"
    PASS_REGULAR_EXPRESSION "Compounded OIS contract verified: D=30,365,1825")
