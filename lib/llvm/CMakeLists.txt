cmake_minimum_required(VERSION 3.4.3)
project(SimpleProject)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set (CMAKE_CXX_STANDARD 14)

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

message(${LLVM_INCLUDE_DIRS})
message(${LLVM_DEFINITIONS})
message(${LLVM_LIBRARY_DIRS})

# Now build our tools
add_executable(simple-tool tool.cpp)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader executionengine interpreter mc nativecodegen)

message(${llvm_libs})

# Link against LLVM libraries
target_link_libraries(simple-tool ${llvm_libs})
