cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED DAL_INSTALL_PREFIX)
    message(FATAL_ERROR "DAL_INSTALL_PREFIX must name an installed DAL prefix")
endif()

if(NOT DEFINED DAL_CONSUMER_BINARY_DIR)
    set(DAL_CONSUMER_BINARY_DIR "${CMAKE_CURRENT_LIST_DIR}/build")
endif()

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}"
    -B "${DAL_CONSUMER_BINARY_DIR}"
    "-DCMAKE_PREFIX_PATH=${DAL_INSTALL_PREFIX}")
if(DEFINED DAL_GENERATOR)
    list(APPEND configure_command -G "${DAL_GENERATOR}")
endif()
if(DEFINED DAL_BUILD_CONFIG)
    list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${DAL_BUILD_CONFIG}")
endif()

execute_process(COMMAND ${configure_command} RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Installed DAL consumer configure failed: ${configure_result}")
endif()

set(build_command "${CMAKE_COMMAND}" --build "${DAL_CONSUMER_BINARY_DIR}")
if(DEFINED DAL_BUILD_CONFIG)
    list(APPEND build_command --config "${DAL_BUILD_CONFIG}")
endif()
execute_process(COMMAND ${build_command} RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Installed DAL consumer build failed: ${build_result}")
endif()

set(test_command "${CMAKE_CTEST_COMMAND}" --test-dir "${DAL_CONSUMER_BINARY_DIR}" --output-on-failure)
if(DEFINED DAL_BUILD_CONFIG)
    list(APPEND test_command -C "${DAL_BUILD_CONFIG}")
endif()
execute_process(COMMAND ${test_command} RESULT_VARIABLE test_result)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Installed DAL consumer test failed: ${test_result}")
endif()
