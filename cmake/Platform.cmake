# Platform-specific flags and settings
if (MSVC)
    # See cmake policy CMP00091
    # One of "MultiThreaded", "MultiThreadedDebug", "MultiThreadedDLL", "MultiThreadedDebugDLL"
    set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${BUILD_SHARED_LIBS}>:DLL>")

    # Export all symbols so MSVC can populate the .lib and .dll
    if (BUILD_SHARED_LIBS)
        # Temp: disable DLL builds on MSVC
        message(FATAL_ERROR
                "Shared library (DLL) builds for DAL on MSVC are not supported")
        set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    INCLUDE_DIRECTORIES($ENV{GTEST_ROOT}/include)

    set(MSVC_RUNTIME "dynamic" CACHE STRING "MSVC runtime to link")
    set_property(CACHE MSVC_RUNTIME PROPERTY STRINGS static dynamic)

    if ("${MSVC_RUNTIME}" STREQUAL "static")
        link_directories($ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE}/MD)
    else()
        link_directories($ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE}/MT)
    endif()

    add_compile_definitions(NOMINMAX)

    # /wd4267
    # Suppress warnings: assignment of 64-bit value to 32-bit QuantLib::Integer (x64)

    # /wd26812
    # Suppress warnings: "Prefer enum class over enum" (Enum.3)

    add_compile_options(/wd4267 /wd26812)
    # Remove warnings
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()