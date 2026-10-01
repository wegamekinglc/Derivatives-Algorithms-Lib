cmake_minimum_required(VERSION 3.21)

set(cases public core_angle core_quoted core_relative core_link legacy_link)
foreach(case IN LISTS cases)
    set(case_dir "${DAL_BOUNDARY_BINARY_DIR}/${case}")
    file(MAKE_DIRECTORY "${case_dir}/src" "${case_dir}/auto")
    set(source "#include <dal-public/src/types.hpp>\n")
    set(generated "")
    set(dependency "DAL::public")
    if(case STREQUAL "core_angle")
        set(source "# include <dal/time/date.hpp>\n")
    elseif(case STREQUAL "core_quoted")
        set(generated "#include \"dal/storage/_repository.hpp\"\n")
    elseif(case STREQUAL "core_relative")
        set(source "#include \"../../dal-cpp/dal/time/date.hpp\"\n")
    elseif(case STREQUAL "core_link")
        set(dependency "DAL::public DAL::cpp")
    elseif(case STREQUAL "legacy_link")
        set(dependency "dal_library")
    endif()
    file(WRITE "${case_dir}/src/binding.cpp" "${source}")
    file(WRITE "${case_dir}/auto/generated.inc" "${generated}")
    file(WRITE "${case_dir}/CMakeLists.txt"
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(BindingBoundary LANGUAGES CXX)\n"
        "include(\"${DAL_BOUNDARY_MODULE}\")\n"
        "add_library(DAL::public INTERFACE IMPORTED)\n"
        "add_library(DAL::cpp INTERFACE IMPORTED)\n"
        "set_target_properties(DAL::public PROPERTIES INTERFACE_LINK_LIBRARIES DAL::cpp)\n"
        "add_library(binding STATIC src/binding.cpp)\n"
        "target_link_libraries(binding PRIVATE ${dependency})\n"
        "dal_public_check_binding(binding \"${case_dir}\")\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${case_dir}" -B "${case_dir}/build"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(case STREQUAL "public")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Public binding rejected: ${output}\n${error}")
        endif()
    elseif(result EQUAL 0 OR NOT error MATCHES "(bindings must|include core contracts)")
        message(FATAL_ERROR "Boundary violation ${case} was not rejected: ${output}\n${error}")
    endif()
endforeach()
