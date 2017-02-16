add_executable(gputester gputester.cc)

target_link_libraries(gputester exys)

file(GLOB TEST_FILES "*.exys")
file(COPY ${TEST_FILES} DESTINATION ".")
