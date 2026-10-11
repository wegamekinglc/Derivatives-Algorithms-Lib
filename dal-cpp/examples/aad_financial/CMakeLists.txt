add_executable(aad_financial aad_financial.cpp)
target_link_libraries(aad_financial PRIVATE DAL::public)
install(TARGETS aad_financial RUNTIME DESTINATION bin)
