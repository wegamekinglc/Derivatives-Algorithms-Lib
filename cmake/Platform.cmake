# Platform-specific flags and settings
if (MSVC)
    # See cmake policy CMP00091
    # One of "MultiThreaded", "MultiThreadedDebug", "MultiThreadedDLL", "MultiThreadedDebugDLL"
    # Export all symbols so MSVC can populate the .lib and .dll
    if (BUILD_SHARED_LIBS)
        # Temp: disable DLL builds on MSVC
        message(FATAL_ERROR
                "Shared library (DLL) builds for DAL on MSVC are not supported")
        set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    if ("${MSVC_RUNTIME}" STREQUAL "static")
        set(USE_MSVC_DYNAMIC_RUNTIME false)
    else()
        set(USE_MSVC_DYNAMIC_RUNTIME true)
    endif()

    message("-- MSVC_RUNTIME: ${USE_MSVC_DYNAMIC_RUNTIME}")
    set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${USE_MSVC_DYNAMIC_RUNTIME}>:DLL>")

    add_compile_definitions(NOMINMAX)

    # /wd4267
    # Suppress warnings: assignment of 64-bit value to 32-bit QuantLib::Integer (x64)

    # /wd26812
    # Suppress warnings: "Prefer enum class over enum" (Enum.3)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        SET(MSVC_COMPILER_OPTION /wd4267 /wd26812)
    else()
        SET(MSVC_COMPILER_OPTION /wd4267 /wd26812 /Qpar /Gy /arch:AVX2 /Oi /GL /Ot /Oy)
    endif()
    add_compile_options(${MSVC_COMPILER_OPTION})
    message("-- MSVC COMPILER OPTION: ${MSVC_COMPILER_OPTION}")

    # Remove warnings
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    if ("${SKIP_TESTS}" STREQUAL "false")
        set(CMAKE_CXX_FLAGS "-march=native -fprofile-instr-generate -fcoverage-mapping")
    else()
        set(CMAKE_CXX_FLAGS "-march=native")
    endif()
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    if ("${SKIP_TESTS}" STREQUAL "false")
        set(CMAKE_CXX_FLAGS "-march=native -ftest-coverage -fprofile-arcs")
    else()
        set(CMAKE_CXX_FLAGS "-march=native")
    endif()
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()