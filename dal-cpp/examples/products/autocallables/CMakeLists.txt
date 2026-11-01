set(AUTOCALLABLE_TYPES
    classic_snowball
    stepdown_snowball
    lockout_snowball
    phoenix
    memory_phoenix
    stepdown_memory_phoenix
    airbag_snowball
    capital_protected
    upside_participation
    worst_of_phoenix)

add_custom_target(autocallables)

add_library(autocallable_example_support STATIC common.cpp)
target_link_libraries(autocallable_example_support PUBLIC DAL::cpp)
dal_apply_platform_options(autocallable_example_support)

foreach(product_type IN LISTS AUTOCALLABLE_TYPES)
    set(example_target autocallable_${product_type})
    add_executable(${example_target} ${product_type}.cpp)
    add_dependencies(autocallables ${example_target})
    target_link_libraries(${example_target} PRIVATE autocallable_example_support)
    dal_apply_platform_options(${example_target})
    install(TARGETS ${example_target} RUNTIME DESTINATION bin)
    add_test(NAME ${example_target} COMMAND ${example_target} 1024)
    set_tests_properties(${example_target} PROPERTIES LABELS examples TIMEOUT 30)
endforeach()
