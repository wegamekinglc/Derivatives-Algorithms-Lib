file(GLOB_RECURSE XCCY_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(xccy_perf ${XCCY_PERF_FILES})

target_link_libraries(xccy_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(xccy_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(xccy_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(xccy_perf adept)
endif()

if(NOT MSVC)
    target_link_libraries(xccy_perf pthread)
endif()

install(TARGETS xccy_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
