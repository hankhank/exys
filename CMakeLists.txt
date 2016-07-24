cmake_minimum_required(VERSION 2.8.11)

project(testproject)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Tell CMake to create the helloworld executable
set (CMAKE_CXX_STANDARD 14)
add_executable(exys test.cc exys.cc node.cc parser.cc)

