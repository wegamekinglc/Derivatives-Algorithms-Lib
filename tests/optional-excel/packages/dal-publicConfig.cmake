file(APPEND "${CMAKE_BINARY_DIR}/package-probes.txt" "dal-public\n")
add_library(DAL::public INTERFACE IMPORTED)
include("${DAL_OPTIONAL_EXCEL_ROOT}/dal-public/cmake/DALBindingBoundary.cmake")

# Configure-only fixture: use the actual installed runtime helper definition.
set(DAL_CPP_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
file(READ "${DAL_OPTIONAL_EXCEL_ROOT}/dal-cpp/cmake/dal-cppConfig.cmake.in" config)
string(REGEX MATCH "function\\(dal_cpp_apply_msvc_runtime target\\)(.|\n)*endfunction\\(\\)" helper "${config}")
file(WRITE "${CMAKE_BINARY_DIR}/runtime-helper.cmake" "${helper}\n")
include("${CMAKE_BINARY_DIR}/runtime-helper.cmake")
