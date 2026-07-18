# Platform-specific settings for DAL-owned targets.

option(DAL_ENABLE_NATIVE_ARCH "Optimize DAL for the build machine's CPU" OFF)

if(MSVC AND BUILD_SHARED_LIBS)
    message(FATAL_ERROR "Shared library (DLL) builds for DAL on MSVC are not supported")
endif()

# Fail fast with an actionable message when a required git submodule was not
# initialized, instead of a bare "does not contain a CMakeLists.txt" error.
function(dal_require_submodule sentinel)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${sentinel}")
        message(FATAL_ERROR
            "Required git submodule sources are missing: "
            "${PROJECT_SOURCE_DIR}/${sentinel}\n"
            "Initialize them with: git submodule update --init --recursive")
    endif()
endfunction()

function(dal_apply_platform_options target)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            _SCL_SECURE_NO_WARNINGS
            _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE
            /wd4267
            /wd26812
            /utf-8
            "$<$<CONFIG:Release>:/O2;/Oi;/Gy;/fp:contract>")

        if(DAL_ENABLE_NATIVE_ARCH)
            target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:/arch:AVX2>")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # Keep floating-point contraction consistent across GCC and Clang. Release
        # optimization is configuration-scoped, and the default CPU baseline stays
        # portable for installed libraries and Python wheels.
        target_compile_options(${target} PRIVATE
            -ffp-contract=fast
            "$<$<CONFIG:Release>:-O3>")

        if(DAL_ENABLE_NATIVE_ARCH)
            target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:-march=native>")
        endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            include(CheckCXXCompilerFlag)
            check_cxx_compiler_flag(
                "-Wno-error=invalid-feature-combination"
                CLANG_HAS_WNO_ERROR_INVALID_FEATURE_COMBINATION)
            if(CLANG_HAS_WNO_ERROR_INVALID_FEATURE_COMBINATION)
                target_compile_options(${target} PRIVATE -Wno-error=invalid-feature-combination)
            endif()
        endif()
    endif()
endfunction()
