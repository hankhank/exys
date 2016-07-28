cmake_minimum_required(VERSION 2.8.11)

project(testproject)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Tell CMake to create the helloworld executable
set (CMAKE_CXX_STANDARD 14)
add_executable(exys test.cc exys.cc graph.cc parser.cc)

find_program(iwyu_path NAMES include-what-you-use iwyu)
if(NOT iwyu_path)
  message(FATAL_ERROR "Could not find the program include-what-you-use")
endif()

set_property(TARGET exys PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})

