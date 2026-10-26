add_executable(native_query_comparison queryworker.cpp)

target_link_libraries(native_query_comparison dal_library QuantLib::QuantLib)
target_include_directories(native_query_comparison PRIVATE "${PROJECT_SOURCE_DIR}/externals/rapidjson/include")

if(NOT MSVC)
    target_link_libraries(native_query_comparison pthread)
endif()
