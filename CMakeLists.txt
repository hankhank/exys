cmake_minimum_required(VERSION 2.8.11)

project(EXYS)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Tell CMake to create the helloworld executable
set (CMAKE_CXX_STANDARD 14)

add_subdirectory(lib)
add_subdirectory(editor)
