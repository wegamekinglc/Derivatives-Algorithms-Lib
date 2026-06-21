

include ("${CMAKE_CURRENT_LIST_DIR}/RapidJSON-targets.cmake")

################################################################################
# RapidJSON source dir
set( RapidJSON_SOURCE_DIR "D:/dev/github/Derivatives-Algorithms-Lib/dal-cpp/externals/rapidjson")

################################################################################
# RapidJSON build dir
set( RapidJSON_DIR "D:/dev/github/Derivatives-Algorithms-Lib/cmake-build-release-windows/dal-cpp/externals/rapidjson")

################################################################################
# Compute paths
get_filename_component(RapidJSON_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

get_target_property(RapidJSON_INCLUDE_DIR RapidJSON INTERFACE_INCLUDE_DIRECTORIES)

set( RapidJSON_INCLUDE_DIRS ${RapidJSON_INCLUDE_DIR} )

if(NOT TARGET rapidjson)
  add_library(rapidjson ALIAS RapidJSON)
endif()
