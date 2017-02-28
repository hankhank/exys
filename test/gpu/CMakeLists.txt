
add_custom_command(
    OUTPUT kernel_gen.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cu kernel.cu
    COMMAND nvcc ${CMAKE_CXX_FLAGS} --device-c kernel.cu -o kernel.co
    COMMAND xxd -i kernel.co kernel_gen.h
    DEPENDS kernel.cu
)

add_executable(gputester gputester.cc kernel_gen.h)

target_link_libraries(gputester exys)

file(GLOB TEST_FILES "*.exys")
file(COPY ${TEST_FILES} DESTINATION ".")
