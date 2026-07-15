add_subdirectory(aad)
add_subdirectory(digital)
add_subdirectory(concurrency)
add_subdirectory(curve_calibration)
add_subdirectory(euribor3m_curve)
add_subdirectory(interpolate_curve)
add_subdirectory(joint_multi_curve_calibration)
add_subdirectory(xccy_curve_calibration)
add_subdirectory(xccy_mtm_calibration)
add_subdirectory(xccy_reset_pricing)
add_subdirectory(european_mc)
add_subdirectory(european_fd)
add_subdirectory(snowball)
add_subdirectory(sobol)
add_subdirectory(script)
add_subdirectory(underdetermined)
add_subdirectory(uoc)
add_subdirectory(vanilla)
add_subdirectory(yield_curve_jacobian)

if(DAL_HAS_EXCEL)
    add_subdirectory(excelwriter)
endif()

# Keep FP-contraction policy consistent with the library, tests, and benchmarks
# (see dal_apply_platform_options in cmake/Platform.cmake).
set(DAL_EXAMPLE_TARGETS
    aad
    digital
    concurrency
    curve_calibration
    euribor3m_curve
    interpolate_curve
    joint_multi_curve_calibration
    xccy_curve_calibration
    xccy_mtm_calibration
    xccy_reset_pricing
    european_mc
    european_fd
    snowball
    sobol
    script
    underdetermined
    uoc
    vanilla
    yield_curve_jacobian)

if(DAL_HAS_EXCEL)
    list(APPEND DAL_EXAMPLE_TARGETS excelwriter)
endif()

foreach(example_target IN LISTS DAL_EXAMPLE_TARGETS)
    dal_apply_platform_options(${example_target})
endforeach()
