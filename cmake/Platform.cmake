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

    INCLUDE_DIRECTORIES($ENV{GTEST_ROOT}/include)

    set(MSVC_RUNTIME "dynamic" CACHE STRING "MSVC runtime to link")
    set_property(CACHE MSVC_RUNTIME PROPERTY STRINGS static dynamic)

    if ("${MSVC_RUNTIME}" STREQUAL "static")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            # 64 bits
            link_directories($ENV{GTEST_ROOT}/lib/Win64/${CMAKE_BUILD_TYPE}/MT)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            # 32 bits
            link_directories($ENV{GTEST_ROOT}/lib/Win32/${CMAKE_BUILD_TYPE}/MT)
        endif()

        set(USE_MSVC_DYNAMIC_RUNTIME false)
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            # 64 bits
            link_directories($ENV{GTEST_ROOT}/lib/Win64/${CMAKE_BUILD_TYPE}/MD)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            # 32 bits
            link_directories($ENV{GTEST_ROOT}/lib/Win32/${CMAKE_BUILD_TYPE}/MD)
        endif()

        set(USE_MSVC_DYNAMIC_RUNTIME true)
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${USE_MSVC_DYNAMIC_RUNTIME}>:DLL>")

    add_compile_definitions(NOMINMAX)

    # /wd4267
    # Suppress warnings: assignment of 64-bit value to 32-bit QuantLib::Integer (x64)

    # /wd26812
    # Suppress warnings: "Prefer enum class over enum" (Enum.3)

    add_compile_options(/wd4267 /wd26812)
    # Remove warnings
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()