cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR "This verification requires a real Windows/MSVC host")
endif()
if(NOT DEFINED DAL_INSTALL_PREFIX OR NOT DEFINED DAL_OPTIONAL_EXCEL_BINARY_DIR)
    message(FATAL_ERROR "DAL_INSTALL_PREFIX and DAL_OPTIONAL_EXCEL_BINARY_DIR are required")
endif()
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(audit "${CMAKE_CURRENT_LIST_DIR}/windows-installed-audit.cmake")
set(gtest_prefix "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/gtest-install")
set(gtest_build "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/gtest-build")
# Only remove this driver's own cases, never the supplied DAL installation.
file(REMOVE_RECURSE "${gtest_prefix}" "${gtest_build}"
    "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/tests-OFF" "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/tests-ON")
file(MAKE_DIRECTORY "${DAL_OPTIONAL_EXCEL_BINARY_DIR}")

function(run_step name)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/${name}.log" "${output}\n${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name} failed (${result}): ${output}\n${error}")
    endif()
    message(STATUS "${name}: passed")
endfunction()

foreach(tests OFF ON)
    if(tests)
        # The workspace intentionally does not install GTest. Build the pinned
        # submodule with the runtime published by the real installed DAL package.
        file(READ "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/tests-OFF/runtime-contract.txt" runtime)
        run_step(gtest-configure "${CMAKE_COMMAND}"
            -S "${repository}/dal-cpp/externals/googletest" -B "${gtest_build}" -G Ninja
            -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=${gtest_prefix}"
            -DCMAKE_INSTALL_LIBDIR=lib "-DCMAKE_MSVC_RUNTIME_LIBRARY=${runtime}"
            -DBUILD_SHARED_LIBS=OFF -DBUILD_GMOCK=OFF -DINSTALL_GTEST=ON
            -Dgtest_force_shared_crt=OFF)
        run_step(gtest-build "${CMAKE_COMMAND}" --build "${gtest_build}" --parallel)
        run_step(gtest-install "${CMAKE_COMMAND}" --install "${gtest_build}")
    endif()

    set(excel_build "${DAL_OPTIONAL_EXCEL_BINARY_DIR}/tests-${tests}")
    run_step("excel-${tests}-configure" "${CMAKE_COMMAND}"
        -S "${repository}/dal-excel" -B "${excel_build}" -G Ninja
        -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=${DAL_INSTALL_PREFIX}"
        "-DDAL_INSTALL_PREFIX=${DAL_INSTALL_PREFIX}" "-DDAL_GTEST_INSTALL_PREFIX=${gtest_prefix}"
        "-DGTest_DIR=${gtest_prefix}/lib/cmake/GTest"
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
        "-DCMAKE_PROJECT_INCLUDE=${audit}" "-DDAL_EXCEL_BUILD_TESTS=${tests}")
    run_step("excel-${tests}-build" "${CMAKE_COMMAND}" --build "${excel_build}" --parallel)
    if(NOT EXISTS "${excel_build}/dal_excel.xll")
        message(FATAL_ERROR "Missing standalone XLL with tests ${tests}")
    endif()
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${excel_build}"
        -C Release --show-only=json-v1 RESULT_VARIABLE list_result OUTPUT_VARIABLE test_list)
    if(NOT list_result EQUAL 0)
        message(FATAL_ERROR "Could not list standalone Excel tests")
    endif()
    string(JSON test_count LENGTH "${test_list}" tests)
    if((tests AND test_count EQUAL 0) OR (NOT tests AND NOT test_count EQUAL 0))
        message(FATAL_ERROR "Unexpected CTest count ${test_count} with tests ${tests}")
    endif()
    run_step("excel-${tests}-ctest" "${CMAKE_CTEST_COMMAND}" --test-dir "${excel_build}"
        -C Release --output-on-failure)
    file(READ "${excel_build}/installed-audit.log" audit_result)
    message(STATUS "Standalone Excel tests ${tests}: XLL present, CTest count=${test_count}\n${audit_result}")
endforeach()
