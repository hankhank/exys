add_library(exys STATIC interpreter.cc gputer.cc jitter.cc graph.cc parser.cc)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader executionengine interpreter mcjit mc nativecodegen nvptxcodegen bitreader asmparser)
message(STATUS "Using LLVMConfig.cmake in: ${llvm_libs}")

target_include_directories (exys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries (exys ${llvm_libs} )
