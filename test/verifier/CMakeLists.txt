add_executable(verifier verifier.cc)

target_link_libraries(verifier exys)

file(GLOB TEST_FILES "*.exys")
file(COPY ${TEST_FILES} DESTINATION ".")
