file(GLOB_RECURSE YCINSTRUMENT_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(ycinstrument_perf ${YCINSTRUMENT_PERF_FILES})

target_link_libraries(ycinstrument_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(ycinstrument_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(ycinstrument_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(ycinstrument_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(ycinstrument_perf pthread)
endif()

install(TARGETS ycinstrument_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
