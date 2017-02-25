add_executable(gputester gputester.cc)

### Find CUDA libs/tools
#find_library(CUDA_LIB cuda ${CUDA_LIB_SEARCH_PATH})
#if(NOT CUDA_LIB)
#  message(FATAL_ERROR "Unable to locate CUDA library/framework")
#endif()
#find_file(CUDA_H cuda.h PATHS "${CUDA_HOME}" "${CUDA_HOME}/include" /usr/local/cuda/include)
#if(CUDA_H)
#  get_filename_component(CUDA_INCLUDE_DIR ${CUDA_H} PATH)
#  include_directories(${CUDA_INCLUDE_DIR})
#else()
#  message(FATAL_ERROR "Unable to locate cuda.h")
#endif()
#message(STATUS "Using cuda header: ${CUDA_H}")

#target_link_libraries(gputester exys ${CUDA_LIB})
target_link_libraries(gputester exys )

file(GLOB TEST_FILES "*.exys")
file(COPY ${TEST_FILES} DESTINATION ".")
