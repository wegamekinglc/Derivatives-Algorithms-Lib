add_executable(aad_public_risk aad_public_risk.cpp)
target_link_libraries(aad_public_risk PRIVATE DAL::public)
install(TARGETS aad_public_risk RUNTIME DESTINATION bin)
