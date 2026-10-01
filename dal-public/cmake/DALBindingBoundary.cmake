include_guard(GLOBAL)

function(dal_public_check_binding target source_root)
    get_target_property(direct_links ${target} LINK_LIBRARIES)
    if(NOT "DAL::public" IN_LIST direct_links AND NOT "dal_public" IN_LIST direct_links)
        message(FATAL_ERROR "${target}: bindings must link DAL::public")
    endif()
    foreach(dependency IN LISTS direct_links)
        if(dependency MATCHES "(^|[:>])DAL::cpp($|[>])|^dal_cpp$|^dal_library$")
            message(FATAL_ERROR "${target}: bindings must not link dal-cpp directly")
        endif()
    endforeach()

    file(GLOB_RECURSE binding_files CONFIGURE_DEPENDS
        "${source_root}/src/*.cpp" "${source_root}/src/*.cc"
        "${source_root}/src/*.hpp" "${source_root}/src/*.h"
        "${source_root}/auto/*.inc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${binding_files})
    foreach(binding_file IN LISTS binding_files)
        file(STRINGS "${binding_file}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
        foreach(include IN LISTS includes)
            string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"].*$" "\\1" include_path "${include}")
            if(include_path MATCHES "(^|/)(dal|dal-cpp)/")
                message(FATAL_ERROR "${target}: include core contracts through dal-public: ${binding_file}: ${include}")
            endif()
        endforeach()
    endforeach()
endfunction()
