cmake_minimum_required(VERSION 3.21.0)

function(dal35_fail category detail)
    message(FATAL_ERROR "DAL35_SCOPE_GUARD:${category}:${detail}")
endfunction()

function(dal35_git_show revision path output)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${DAL35_REPOSITORY}" show "${revision}:${path}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE contents
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        dal35_fail(repository_read "${revision}:${path}:${error}")
    endif()
    set("${output}" "${contents}" PARENT_SCOPE)
endfunction()

function(dal35_extract_range contents begin_token end_token category output)
    string(FIND "${contents}" "${begin_token}" begin_index)
    if(begin_index EQUAL -1)
        dal35_fail("${category}" "missing_begin")
    endif()
    string(SUBSTRING "${contents}" "${begin_index}" -1 tail)
    string(FIND "${tail}" "${end_token}" end_index)
    if(end_index EQUAL -1)
        dal35_fail("${category}" "missing_end")
    endif()
    string(LENGTH "${end_token}" end_length)
    math(EXPR range_length "${end_index} + ${end_length}")
    string(SUBSTRING "${tail}" 0 "${range_length}" range)
    set("${output}" "${range}" PARENT_SCOPE)
endfunction()

function(dal35_validate_marker contents marker)
    string(REGEX MATCHALL "(^|\n)[ \t]*${marker}[ \t]*(\n|$)" matches "${contents}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        dal35_fail(marker "${marker}:count=${count}")
    endif()
endfunction()

function(dal35_canonicalize_block contents begin_marker end_marker sentinel output)
    dal35_validate_marker("${contents}" "${begin_marker}")
    dal35_validate_marker("${contents}" "${end_marker}")
    string(FIND "${contents}" "${begin_marker}" begin_index)
    string(FIND "${contents}" "${end_marker}" end_index)
    if(end_index LESS begin_index)
        dal35_fail(marker "${begin_marker}:order")
    endif()
    string(LENGTH "${begin_marker}" begin_length)
    math(EXPR body_begin "${begin_index} + ${begin_length}")
    string(SUBSTRING "${contents}" 0 "${body_begin}" prefix)
    string(SUBSTRING "${contents}" "${end_index}" -1 suffix)
    set("${output}" "${prefix}\n${sentinel}\n${suffix}" PARENT_SCOPE)
endfunction()

function(dal35_collect_lines contents expression output)
    string(REGEX MATCHALL "(^|\n)[^\n]*(${expression})[^\n]*" matches "${contents}")
    string(JOIN "\n" signature ${matches})
    set("${output}" "${signature}" PARENT_SCOPE)
endfunction()

function(dal35_fail_block_line category line)
    if(line MATCHES "\\(dal_cpp([ \t]|\\)|$)" OR line MATCHES "\\(dal_cpp_tests([ \t]|\\)|$)")
        dal35_fail(normal_target "${line}")
    endif()
    dal35_fail("${category}" "${line}")
endfunction()

function(dal35_validate_source_block contents)
    dal35_extract_range(
        "${contents}" "${DAL35_SOURCE_BEGIN}" "${DAL35_SOURCE_END}"
        source_block_scope block)
    string(REPLACE "\n" ";" lines "${block}")
    set(seen_set FALSE)
    set(open_set FALSE)
    foreach(raw_line IN LISTS lines)
        string(STRIP "${raw_line}" line)
        if(NOT line OR line STREQUAL "${DAL35_SOURCE_BEGIN}" OR line STREQUAL "${DAL35_SOURCE_END}")
            continue()
        endif()
        if(NOT seen_set)
            if(line STREQUAL "set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)")
                set(seen_set TRUE)
            elseif(line STREQUAL "set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES")
                set(seen_set TRUE)
                set(open_set TRUE)
            else()
                dal35_fail_block_line(source_block_scope "${line}")
            endif()
        elseif(open_set)
            if(line STREQUAL ")")
                set(open_set FALSE)
            elseif(line MATCHES "\\$<")
                dal35_fail_block_line(source_block_scope "${line}")
            elseif(line MATCHES "^\"[^\"]+\"\\)$")
                set(open_set FALSE)
            elseif(NOT line MATCHES "^\"[^\"]+\"$")
                dal35_fail_block_line(source_block_scope "${line}")
            endif()
        else()
            dal35_fail_block_line(source_block_scope "${line}")
        endif()
    endforeach()
    if(NOT seen_set OR open_set)
        dal35_fail(source_block_scope "incomplete_source_set")
    endif()
endfunction()

function(dal35_validate_ctest_block contents)
    dal35_extract_range(
        "${contents}" "${DAL35_CTEST_BEGIN}" "${DAL35_CTEST_END}"
        ctest_block_scope block)
    string(REPLACE "\n" ";" lines "${block}")
    foreach(raw_line IN LISTS lines)
        string(STRIP "${raw_line}" line)
        if(NOT line OR line STREQUAL "${DAL35_CTEST_BEGIN}" OR line STREQUAL "${DAL35_CTEST_END}")
            continue()
        endif()
        if(NOT line MATCHES "^_dal_cpp_register_dal35_seam_test\\(\"[^\"]+\"\\)$")
            dal35_fail_block_line(ctest_block_scope "${line}")
        endif()
    endforeach()
endfunction()

get_filename_component(DAL35_DEFAULT_REPOSITORY "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DAL35_REPOSITORY "${DAL35_DEFAULT_REPOSITORY}")
set(DAL35_BASE "")
set(DAL35_HEAD "")
set(DAL35_PENDING_ARGUMENT "")

math(EXPR DAL35_LAST_ARGUMENT "${CMAKE_ARGC} - 1")
foreach(index RANGE 0 "${DAL35_LAST_ARGUMENT}")
    set(argument "${CMAKE_ARGV${index}}")
    if(DAL35_PENDING_ARGUMENT)
        set("${DAL35_PENDING_ARGUMENT}" "${argument}")
        set(DAL35_PENDING_ARGUMENT "")
    elseif(argument STREQUAL "--repo")
        set(DAL35_PENDING_ARGUMENT DAL35_REPOSITORY)
    elseif(argument STREQUAL "--base")
        set(DAL35_PENDING_ARGUMENT DAL35_BASE)
    elseif(argument STREQUAL "--head")
        set(DAL35_PENDING_ARGUMENT DAL35_HEAD)
    endif()
endforeach()

if(DAL35_PENDING_ARGUMENT)
    dal35_fail(arguments "missing_value")
endif()
if(NOT DAL35_BASE OR NOT DAL35_HEAD)
    dal35_fail(arguments "--base and --head are required")
endif()

find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${DAL35_REPOSITORY}" rev-parse --show-toplevel
    RESULT_VARIABLE repository_result
    OUTPUT_VARIABLE repository_root
    ERROR_VARIABLE repository_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT repository_result EQUAL 0)
    dal35_fail(repository "${repository_error}")
endif()
get_filename_component(DAL35_REPOSITORY "${DAL35_REPOSITORY}" ABSOLUTE)
get_filename_component(repository_root "${repository_root}" ABSOLUTE)
if(NOT DAL35_REPOSITORY STREQUAL repository_root)
    dal35_fail(repository "not_toplevel:${DAL35_REPOSITORY}")
endif()

foreach(revision IN ITEMS "${DAL35_BASE}" "${DAL35_HEAD}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${DAL35_REPOSITORY}" rev-parse --verify "${revision}^{commit}"
        RESULT_VARIABLE revision_result
        OUTPUT_QUIET
        ERROR_VARIABLE revision_error)
    if(NOT revision_result EQUAL 0)
        dal35_fail(revision "${revision}:${revision_error}")
    endif()
endforeach()

set(DAL35_CMAKE_PATH "dal-cpp/CMakeLists.txt")
set(DAL35_TEST_PATH "dal-cpp/tests/math/matrix/test_bcg.cpp")
set(DAL35_GUARD_PATH "dal-cpp/cmake/dal35-private-target-scope-guard.cmake")
set(DAL35_SOURCE_BEGIN "# BEGIN DAL58_P4_PRIVATE_TARGET_SOURCES")
set(DAL35_SOURCE_END "# END DAL58_P4_PRIVATE_TARGET_SOURCES")
set(DAL35_CTEST_BEGIN "# BEGIN DAL58_P4_PRIVATE_TARGET_CTEST")
set(DAL35_CTEST_END "# END DAL58_P4_PRIVATE_TARGET_CTEST")
set(DAL35_TEST_NAME "TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary")
set(DAL35_TEST_FILTER "MatrixTest.${DAL35_TEST_NAME}")

dal35_git_show("${DAL35_BASE}" "${DAL35_CMAKE_PATH}" base_cmake)
dal35_git_show("${DAL35_HEAD}" "${DAL35_CMAKE_PATH}" head_cmake)
dal35_git_show("${DAL35_BASE}" "${DAL35_TEST_PATH}" base_test)
dal35_git_show("${DAL35_HEAD}" "${DAL35_TEST_PATH}" head_test)
dal35_git_show("${DAL35_BASE}" "${DAL35_GUARD_PATH}" base_guard)
dal35_git_show("${DAL35_HEAD}" "${DAL35_GUARD_PATH}" head_guard)

if(NOT base_guard STREQUAL head_guard)
    dal35_fail(guard_implementation "${DAL35_GUARD_PATH}")
endif()

foreach(contents IN ITEMS "${base_cmake}" "${head_cmake}")
    dal35_validate_marker("${contents}" "${DAL35_SOURCE_BEGIN}")
    dal35_validate_marker("${contents}" "${DAL35_SOURCE_END}")
    dal35_validate_marker("${contents}" "${DAL35_CTEST_BEGIN}")
    dal35_validate_marker("${contents}" "${DAL35_CTEST_END}")
    dal35_validate_source_block("${contents}")
    dal35_validate_ctest_block("${contents}")
endforeach()

set(DAL35_NORMAL_TEST_EXCLUSION
    [=[set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE)
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
        ${DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE})]=])
foreach(contents IN ITEMS "${base_cmake}" "${head_cmake}")
    string(FIND "${contents}" "${DAL35_NORMAL_TEST_EXCLUSION}" exclusion_index)
    if(exclusion_index EQUAL -1)
        dal35_fail(normal_target "private_sources_not_excluded")
    endif()
endforeach()

set(DAL35_TEST_BEGIN "#if defined(DAL35_ENABLE_TEST_SEAM)\nTEST(MatrixTest, ${DAL35_TEST_NAME})")
dal35_extract_range(
    "${base_test}" "${DAL35_TEST_BEGIN}" "\n#endif"
    seam_test_body base_seam_test)
dal35_extract_range(
    "${head_test}" "${DAL35_TEST_BEGIN}" "\n#endif"
    seam_test_body head_seam_test)
if(NOT base_seam_test STREQUAL head_seam_test)
    dal35_fail(seam_test_body "${DAL35_TEST_FILTER}")
endif()

dal35_extract_range(
    "${base_cmake}" "add_executable(dal_cpp_tests" "gtest_discover_tests(dal_cpp_tests)"
    normal_target base_normal_target)
dal35_extract_range(
    "${head_cmake}" "add_executable(dal_cpp_tests" "gtest_discover_tests(dal_cpp_tests)"
    normal_target head_normal_target)
if(NOT base_normal_target STREQUAL head_normal_target)
    dal35_fail(normal_target "dal_cpp_tests")
endif()

dal35_collect_lines("${base_cmake}" "option\\(|[ \t]CACHE[ \t]" base_options)
dal35_collect_lines("${head_cmake}" "option\\(|[ \t]CACHE[ \t]" head_options)
if(NOT base_options STREQUAL head_options)
    dal35_fail(public_cache_option "option_or_cache_changed")
endif()

dal35_collect_lines(
    "${base_cmake}"
    "install\\(|EXPORT|configure_package_config_file\\(|write_basic_package_version_file\\("
    base_install)
dal35_collect_lines(
    "${head_cmake}"
    "install\\(|EXPORT|configure_package_config_file\\(|write_basic_package_version_file\\("
    head_install)
if(NOT base_install STREQUAL head_install)
    dal35_fail(install_export "install_or_export_changed")
endif()

set(DAL35_TESTS_HEADING
    "# ---------------------------------------------------------------------------\n# Tests\n# ---------------------------------------------------------------------------")
dal35_extract_range(
    "${base_cmake}" "add_library(dal_cpp" "${DAL35_TESTS_HEADING}"
    normal_target base_normal_library)
dal35_extract_range(
    "${head_cmake}" "add_library(dal_cpp" "${DAL35_TESTS_HEADING}"
    normal_target head_normal_library)
if(NOT base_normal_library STREQUAL head_normal_library)
    dal35_fail(normal_target "dal_cpp")
endif()

set(DAL35_TARGET_DECLARATION "add_executable(dal_cpp_dal35_seam_tests\n")
string(FIND "${head_cmake}" "${DAL35_TARGET_DECLARATION}" target_index)
if(target_index EQUAL -1)
    dal35_fail(private_target_name "dal_cpp_dal35_seam_tests")
endif()

set(DAL35_SEAM_DEFINITION
    "target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE DAL35_ENABLE_TEST_SEAM)")
string(FIND "${head_cmake}" "${DAL35_SEAM_DEFINITION}" definition_index)
if(definition_index EQUAL -1)
    dal35_fail(seam_definition "DAL35_ENABLE_TEST_SEAM")
endif()

string(FIND "${head_cmake}" "${DAL35_TEST_FILTER}" filter_index)
if(filter_index EQUAL -1)
    dal35_fail(seam_filter "${DAL35_TEST_FILTER}")
endif()

dal35_extract_range(
    "${base_cmake}"
    "set(DAL_CPP_DAL35_SEAM_BASE_SOURCES"
    "${DAL35_SOURCE_BEGIN}"
    seam_sources base_seam_sources)
dal35_extract_range(
    "${head_cmake}"
    "set(DAL_CPP_DAL35_SEAM_BASE_SOURCES"
    "${DAL35_SOURCE_BEGIN}"
    seam_sources head_seam_sources)
if(NOT base_seam_sources STREQUAL head_seam_sources)
    dal35_fail(seam_sources "DAL_CPP_DAL35_SEAM_BASE_SOURCES")
endif()

set(DAL35_LINKAGE
    "target_link_libraries(dal_cpp_dal35_seam_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)")
string(FIND "${head_cmake}" "${DAL35_LINKAGE}" linkage_index)
if(linkage_index EQUAL -1)
    dal35_fail(toolchain_linkage "dal_cpp_and_test_dependencies")
endif()

dal35_extract_range(
    "${head_cmake}" "${DAL35_CTEST_BEGIN}" "${DAL35_CTEST_END}"
    seam_registration head_ctest_block)
set(DAL35_REGISTRATION
    "_dal_cpp_register_dal35_seam_test(\"\${DAL_CPP_DAL35_SEAM_TEST_FILTER}\")")
string(FIND "${head_ctest_block}" "${DAL35_REGISTRATION}" registration_index)
if(registration_index EQUAL -1)
    dal35_fail(seam_registration "${DAL35_TEST_FILTER}")
endif()

dal35_canonicalize_block(
    "${base_cmake}" "${DAL35_SOURCE_BEGIN}" "${DAL35_SOURCE_END}"
    "<DAL58_P4_PRIVATE_TARGET_SOURCES>" base_sources_canonical)
dal35_canonicalize_block(
    "${head_cmake}" "${DAL35_SOURCE_BEGIN}" "${DAL35_SOURCE_END}"
    "<DAL58_P4_PRIVATE_TARGET_SOURCES>" head_sources_canonical)
dal35_canonicalize_block(
    "${base_sources_canonical}" "${DAL35_CTEST_BEGIN}" "${DAL35_CTEST_END}"
    "<DAL58_P4_PRIVATE_TARGET_CTEST>" base_canonical)
dal35_canonicalize_block(
    "${head_sources_canonical}" "${DAL35_CTEST_BEGIN}" "${DAL35_CTEST_END}"
    "<DAL58_P4_PRIVATE_TARGET_CTEST>" head_canonical)
if(NOT base_canonical STREQUAL head_canonical)
    dal35_fail(protected_cmake "${DAL35_CMAKE_PATH}")
endif()

message(STATUS "DAL35_SCOPE_GUARD:pass:${DAL35_BASE}..${DAL35_HEAD}")
