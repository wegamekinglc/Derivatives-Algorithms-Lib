cmake_minimum_required(VERSION 3.21.0)

if(NOT DEFINED DAL35_GUARD_SCRIPT)
    set(DAL35_GUARD_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/dal35-private-target-scope-guard.cmake")
endif()
if(NOT EXISTS "${DAL35_GUARD_SCRIPT}")
    message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:guard_missing:${DAL35_GUARD_SCRIPT}")
endif()

find_package(Git REQUIRED)

if(DEFINED ENV{TMPDIR})
    set(DAL35_TMP_ROOT "$ENV{TMPDIR}")
elseif(DEFINED ENV{TEMP})
    set(DAL35_TMP_ROOT "$ENV{TEMP}")
else()
    set(DAL35_TMP_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef DAL35_FIXTURE_SUFFIX)
set(DAL35_FIXTURE_REPOSITORY "${DAL35_TMP_ROOT}/dal35-scope-guard-${DAL35_FIXTURE_SUFFIX}")

function(dal35_run_git)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${DAL35_FIXTURE_REPOSITORY}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:git_failed:${ARGN}\n${output}\n${error}")
    endif()
    set(DAL35_GIT_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(dal35_commit_case name)
    dal35_run_git(add .)
    dal35_run_git(commit -m "${name}")
    dal35_run_git(rev-parse HEAD)
    string(STRIP "${DAL35_GIT_OUTPUT}" case_head)
    set(DAL35_CASE_HEAD "${case_head}" PARENT_SCOPE)
endfunction()

function(dal35_begin_case)
    dal35_run_git(checkout --detach "${DAL35_BASE_SHA}")
endfunction()

function(dal35_replace path old new)
    set(full_path "${DAL35_FIXTURE_REPOSITORY}/${path}")
    file(READ "${full_path}" contents)
    string(REPLACE "${old}" "${new}" updated "${contents}")
    if(updated STREQUAL contents)
        message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:fixture_replace_missed:${path}:${old}")
    endif()
    file(WRITE "${full_path}" "${updated}")
endfunction()

function(dal35_expect_guard expected_result expected_category)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${DAL35_GUARD_SCRIPT}" --
                --repo "${DAL35_FIXTURE_REPOSITORY}"
                --base "${DAL35_BASE_SHA}"
                --head "${DAL35_CASE_HEAD}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(expected_result STREQUAL "pass")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:expected_pass:${expected_category}\n${combined}")
        endif()
    else()
        if(result EQUAL 0)
            message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:expected_failure:${expected_category}")
        endif()
        if(NOT combined MATCHES "DAL35_SCOPE_GUARD:${expected_category}")
            message(FATAL_ERROR "DAL35_SCOPE_GUARD_TEST:wrong_category:${expected_category}\n${combined}")
        endif()
    endif()
    message(STATUS "DAL35_SCOPE_GUARD_TEST:${expected_result}:${expected_category}")
endfunction()

file(REMOVE_RECURSE "${DAL35_FIXTURE_REPOSITORY}")
file(MAKE_DIRECTORY
    "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/cmake"
    "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/tests/math/matrix")

set(DAL35_FIXTURE_CMAKE [=[
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
add_library(dal_cpp core.cpp)
install(TARGETS dal_cpp EXPORT DALCppTargets)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_TESTS)
    set(DAL_CPP_DAL35_SEAM_TEST_FILTER
        "MatrixTest.TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary")
    set(DAL_CPP_DAL35_SEAM_BASE_SOURCES
        "${PROJECT_SOURCE_DIR}/dal/math/matrix/bcg.cpp"
        "${PROJECT_SOURCE_DIR}/tests/math/matrix/test_bcg.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")

    # BEGIN DAL58_P4_PRIVATE_TARGET_SOURCES
    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)
    # END DAL58_P4_PRIVATE_TARGET_SOURCES

    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE)
    foreach(private_source IN LISTS DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)
        cmake_path(
            ABSOLUTE_PATH private_source
            BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
            NORMALIZE
            OUTPUT_VARIABLE private_source_absolute)
        list(APPEND DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE
            "${private_source_absolute}")
    endforeach()
    list(REMOVE_ITEM TEST_FILES
        ${DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE})

    add_executable(dal_cpp_tests normal.cpp)
    gtest_discover_tests(dal_cpp_tests)

    add_executable(dal_cpp_dal35_seam_tests
        ${DAL_CPP_DAL35_SEAM_BASE_SOURCES}
        ${DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES})
    target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE DAL35_ENABLE_TEST_SEAM)
    target_link_libraries(dal_cpp_dal35_seam_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)

    function(_dal_cpp_register_dal35_seam_test test_filter)
        add_test(
            NAME "dal35_seam::${test_filter}"
            COMMAND dal_cpp_dal35_seam_tests
                    "--gtest_filter=${test_filter}"
                    "--gtest_fail_if_no_test_selected")
    endfunction()

    # BEGIN DAL58_P4_PRIVATE_TARGET_CTEST
    _dal_cpp_register_dal35_seam_test("${DAL_CPP_DAL35_SEAM_TEST_FILTER}")
    # END DAL58_P4_PRIVATE_TARGET_CTEST
endif()
]=])
file(WRITE "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/CMakeLists.txt" "${DAL35_FIXTURE_CMAKE}")

set(DAL35_FIXTURE_TEST [=[
#if defined(DAL35_ENABLE_TEST_SEAM)
TEST(MatrixTest, TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary) {
    ASSERT_EQ(1, dal35ExactWorkspaceConstructionCount_);
}
#endif
]=])
file(WRITE "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/tests/math/matrix/test_bcg.cpp" "${DAL35_FIXTURE_TEST}")
file(COPY_FILE
    "${DAL35_GUARD_SCRIPT}"
    "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/cmake/dal35-private-target-scope-guard.cmake")

dal35_run_git(init)
dal35_run_git(config user.email "dal35-scope-guard@example.invalid")
dal35_run_git(config user.name "DAL35 scope guard")
dal35_commit_case("base")
string(STRIP "${DAL35_CASE_HEAD}" DAL35_BASE_SHA)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES\n        \"tests/math/matrix/test_dal58_exact_alpha.cpp\")")
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    _dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")"
    "    _dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")\n    _dal_cpp_register_dal35_seam_test(\"MatrixTest.TestDal58ExactAlpha\")")
dal35_commit_case("allowed private additions")
dal35_expect_guard(pass allowed_blocks)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES\n        \"\\$<\\$<BOOL:1>:\${PROJECT_SOURCE_DIR}/tests/math/matrix/test_dal58_exact_alpha.cpp>\")")
dal35_commit_case("add generator expression private source")
dal35_expect_guard(fail source_block_scope)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)"
    "    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)\n    target_compile_definitions(dal_cpp PUBLIC DAL35_ENABLE_TEST_SEAM)")
dal35_commit_case("hide normal target mutation in allowed block")
dal35_expect_guard(fail normal_target)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    _dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")"
    "    _dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")\n    target_compile_definitions(dal_cpp_tests PRIVATE DAL35_ENABLE_TEST_SEAM)")
dal35_commit_case("hide normal target mutation in ctest block")
dal35_expect_guard(fail normal_target)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "        cmake_path(\n            ABSOLUTE_PATH private_source\n            BASE_DIRECTORY \"\${PROJECT_SOURCE_DIR}\"\n            NORMALIZE\n            OUTPUT_VARIABLE private_source_absolute)"
    "")
dal35_commit_case("remove private source normalization from normal target")
dal35_expect_guard(fail normal_target)

dal35_begin_case()
dal35_replace("dal-cpp/CMakeLists.txt" "add_executable(dal_cpp_tests normal.cpp)" "add_executable(dal_cpp_tests changed.cpp)")
dal35_commit_case("mutate normal target")
dal35_expect_guard(fail normal_target)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "add_library(dal_cpp core.cpp)"
    "add_library(dal_cpp core.cpp)\ntarget_compile_definitions(dal_cpp PUBLIC DAL35_ENABLE_TEST_SEAM)")
dal35_commit_case("leak seam macro to normal library")
dal35_expect_guard(fail normal_target)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "option(DAL_CPP_BUILD_TESTS \"Build dal-cpp tests\" ON)"
    "option(DAL_CPP_BUILD_TESTS \"Build dal-cpp tests\" ON)\noption(DAL35_PUBLIC_SEAM \"Expose seam\" OFF)")
dal35_commit_case("add public option")
dal35_expect_guard(fail public_cache_option)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "install(TARGETS dal_cpp EXPORT DALCppTargets)"
    "install(TARGETS dal_cpp EXPORT DALCppTargets)\ninstall(TARGETS dal_cpp_dal35_seam_tests)")
dal35_commit_case("install private target")
dal35_expect_guard(fail install_export)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "add_executable(dal_cpp_dal35_seam_tests"
    "add_executable(dal_cpp_dal35_seam_tests_changed")
dal35_commit_case("rename private target")
dal35_expect_guard(fail private_target_name)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE DAL35_ENABLE_TEST_SEAM)"
    "target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE DAL35_ENABLE_TEST_SEAM_CHANGED)")
dal35_commit_case("change seam definition")
dal35_expect_guard(fail seam_definition)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "\"\${PROJECT_SOURCE_DIR}/dal/math/matrix/bcg.cpp\""
    "\"\${PROJECT_SOURCE_DIR}/dal/math/matrix/bcg_changed.cpp\"")
dal35_commit_case("change seam sources")
dal35_expect_guard(fail seam_sources)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "MatrixTest.TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary"
    "MatrixTest.TestChangedWorkspaceConstructionBoundary")
dal35_commit_case("change seam filter")
dal35_expect_guard(fail seam_filter)

dal35_begin_case()
dal35_replace(
    "dal-cpp/tests/math/matrix/test_bcg.cpp"
    "ASSERT_EQ(1, dal35ExactWorkspaceConstructionCount_);"
    "ASSERT_EQ(2, dal35ExactWorkspaceConstructionCount_);")
dal35_commit_case("change seam test body")
dal35_expect_guard(fail seam_test_body)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "# BEGIN DAL58_P4_PRIVATE_TARGET_SOURCES"
    "# BEGIN DAL58_P4_PRIVATE_TARGET_SOURCES_CHANGED")
dal35_commit_case("change marker")
dal35_expect_guard(fail marker)

dal35_begin_case()
file(APPEND
    "${DAL35_FIXTURE_REPOSITORY}/dal-cpp/cmake/dal35-private-target-scope-guard.cmake"
    "\n# changed\n")
dal35_commit_case("change guard implementation")
dal35_expect_guard(fail guard_implementation)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "target_link_libraries(dal_cpp_dal35_seam_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)"
    "target_link_libraries(dal_cpp_dal35_seam_tests PRIVATE gtest gtest_main gmock gmock_main)")
dal35_commit_case("change private linkage")
dal35_expect_guard(fail toolchain_linkage)

dal35_begin_case()
dal35_replace(
    "dal-cpp/CMakeLists.txt"
    "    _dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")"
    "")
dal35_commit_case("remove seam registration")
dal35_expect_guard(fail seam_registration)

file(REMOVE_RECURSE "${DAL35_FIXTURE_REPOSITORY}")
message(STATUS "DAL35_SCOPE_GUARD_TEST:all_cases_passed")
