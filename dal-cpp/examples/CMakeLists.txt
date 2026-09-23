add_subdirectory(aad)
add_subdirectory(digital)
add_subdirectory(concurrency)
add_subdirectory(curve_calibration)
add_subdirectory(euribor3m_curve)
add_subdirectory(interpolate_curve)
add_subdirectory(joint_multi_curve_calibration)
add_subdirectory(quote_risk)
add_subdirectory(xccy_curve_calibration)
add_subdirectory(xccy_mtm_calibration)
add_subdirectory(xccy_reset_pricing)
add_subdirectory(american_put_mc)
add_subdirectory(european_mc)
add_subdirectory(european_fd)
add_subdirectory(fix_mc)
add_subdirectory(snowball)
add_subdirectory(sobol)
add_subdirectory(script)
add_subdirectory(script_tree)
add_subdirectory(underdetermined)
add_subdirectory(uoc)
add_subdirectory(vanilla)
add_subdirectory(yield_curve_jacobian)

# The excelwriter example needs MSVC's #import COM support plus the Microsoft
# Office type libraries on the build machine (see excelimport.hpp). Detect
# them; users may also pass the OFFICE_* paths explicitly as cache variables.
set(DAL_HAS_EXCEL OFF)
if(WIN32 AND MSVC)
    set(_OFFICE_SEARCH_PATHS
        "C:/Program Files/Microsoft Office/root/VFS/ProgramFilesCommonX86/Microsoft Shared/OFFICE16"
        "C:/Program Files (x86)/Common Files/microsoft shared/OFFICE16"
        "C:/Program Files/Common Files/Microsoft Shared/OFFICE16")
    set(_VBA_SEARCH_PATHS
        "C:/Program Files/Microsoft Office/root/VFS/ProgramFilesCommonX86/Microsoft Shared/VBA/VBA6"
        "C:/Program Files (x86)/Common Files/microsoft shared/VBA/VBA6"
        "C:/Program Files/Common Files/Microsoft Shared/VBA/VBA6")
    set(_EXCEL_SEARCH_PATHS
        "C:/Program Files/Microsoft Office/root/Office16"
        "C:/Program Files (x86)/Microsoft Office/Office16"
        "C:/Program Files/Microsoft Office/Office16")
    find_file(OFFICE_MSO_DLL NAMES "MSO.DLL" PATHS ${_OFFICE_SEARCH_PATHS} NO_DEFAULT_PATH)
    find_file(OFFICE_VBE_OLB NAMES "VBE6EXT.OLB" PATHS ${_VBA_SEARCH_PATHS} NO_DEFAULT_PATH)
    find_file(OFFICE_EXCEL_EXE NAMES "EXCEL.EXE" PATHS ${_EXCEL_SEARCH_PATHS} NO_DEFAULT_PATH)
    if(OFFICE_MSO_DLL AND OFFICE_VBE_OLB AND OFFICE_EXCEL_EXE)
        set(DAL_HAS_EXCEL ON)
        message(STATUS "Office found - excelwriter example enabled")
    else()
        message(STATUS "Office not found - excelwriter example skipped")
    endif()
endif()

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
    quote_risk
    xccy_curve_calibration
    xccy_mtm_calibration
    xccy_reset_pricing
    american_put_mc
    european_mc
    european_fd
    fix_mc
    snowball
    sobol
    script
    script_tree
    underdetermined
    uoc
    vanilla
    yield_curve_jacobian)

if(DAL_HAS_EXCEL)
    list(APPEND DAL_EXAMPLE_TARGETS excelwriter)
endif()

# Register examples as CTest smoke tests so CI exercises them at runtime, not
# only at compile time (commit 05cd72c0 fixed a compile-clean example that
# aborted at runtime). An example joins the "examples" label only when it is
# self-contained: exits 0 in well under 30 s with no input files, no network,
# and no file outputs. The uoc smoke test uses fewer paths than its standalone
# default; its full-scale run takes about six minutes with coverage enabled.
# Exclusions:
#   european_mc - ~35 s runtime; runs under "examples_slow", kept out of CI
#   excelwriter - drives a live Excel COM instance (Windows/MSVC only)
set(DAL_EXAMPLE_SLOW_TARGETS european_mc)
set(DAL_EXAMPLE_UNREGISTERED_TARGETS excelwriter)

foreach(example_target IN LISTS DAL_EXAMPLE_TARGETS)
    dal_apply_platform_options(${example_target})
    if(example_target IN_LIST DAL_EXAMPLE_UNREGISTERED_TARGETS)
        continue()
    endif()
    if(example_target IN_LIST DAL_EXAMPLE_SLOW_TARGETS)
        set(example_label examples_slow)
    else()
        set(example_label examples)
    endif()
    if(example_target STREQUAL "uoc")
        add_test(NAME uoc COMMAND uoc 32768)
        set_tests_properties(uoc PROPERTIES TIMEOUT 60)
    else()
        add_test(NAME ${example_target} COMMAND ${example_target})
    endif()
    set_tests_properties(${example_target} PROPERTIES LABELS ${example_label})
endforeach()
