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
