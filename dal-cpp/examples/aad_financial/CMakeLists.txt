add_executable(aad_financial aad_financial.cpp)
target_link_libraries(aad_financial PRIVATE DAL::cpp)
install(TARGETS aad_financial RUNTIME DESTINATION bin)
