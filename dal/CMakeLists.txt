file(GLOB_RECURSE DAL_FILES "*.hpp" "*.cpp")

add_library(dal STATIC ${DAL_FILES})