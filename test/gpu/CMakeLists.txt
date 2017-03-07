
### Find CUDA libs/tools
find_library(CUDA_LIB cuda "/usr/local/cuda/targets/x86_64-linux/lib/stubs")
if(NOT CUDA_LIB)
  message(FATAL_ERROR "Unable to locate CUDA library/framework")
endif()
find_file(CUDA_H cuda.h PATHS "${CUDA_HOME}" "${CUDA_HOME}/include" /usr/local/cuda/include)
if(CUDA_H)
  get_filename_component(CUDA_INCLUDE_DIR ${CUDA_H} PATH)
  include_directories(${CUDA_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Unable to locate cuda.h")
endif()
message(STATUS "Using cuda header: ${CUDA_H}")

add_custom_command(
    OUTPUT kernel_gen.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cu kernel.cu
    COMMAND nvcc -I ../../../lib --std=c++11 --device-c -g kernel.cu -o kernel.co
    COMMAND xxd -i kernel.co kernel_gen.h
    DEPENDS kernel.cu
)

add_executable(gputester gputester.cc kernel_gen.h)

target_link_libraries(gputester exys ${CUDA_LIB})

file(GLOB TEST_FILES "*.exys")
file(COPY ${TEST_FILES} DESTINATION ".")
