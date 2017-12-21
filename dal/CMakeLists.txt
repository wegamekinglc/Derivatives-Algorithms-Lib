file(GLOB_RECURSE DAL_FILES "*.hpp" "*.cpp")


if(MSVC)
    add_library(dal STATIC ${DAL_FILES})
else()
    add_library(dal SHARED ${DAL_FILES})
endif()