cmake_minimum_required(VERSION 2.8.11)

project(EXYS)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Tell CMake to create the helloworld executable
set (CMAKE_CXX_STANDARD 14)

option(JIT "JIT" ON)
if (JIT)
    add_definitions(-DEXYS_JIT=1)
endif()

option(GPU "GPU" ON)
if (GPU)
    add_definitions(-DEXYS_GPU=1)
endif()

add_subdirectory(lib)
add_subdirectory(editor)
add_subdirectory(test)
