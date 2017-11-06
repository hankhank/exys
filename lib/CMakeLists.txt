add_library(exys STATIC interpreter.cc graph.cc parser.cc)

target_include_directories (exys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

if (JIT)
target_sources(exys PRIVATE jitwrap.cc jitter.cc)

if (GPU)
target_sources(exys PRIVATE gputer.cc)
endif()

#set (LLVM_BUILD_ROOT LLVMConfig.cmake /home/andrew/Projects/llvm-3.8.1.src/build/share/llvm/cmake)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader executionengine interpreter mcjit mc nativecodegen nvptxcodegen bitreader asmparser passes)
message(STATUS "Using LLVMConfig.cmake in: ${llvm_libs}")
target_link_libraries (exys ${llvm_libs} )

endif()

