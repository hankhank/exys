
SET(RESOURCE_COMPILER xxd)
FILE(GLOB_RECURSE COMPILED_STD_LIB "std/*.exys")
FOREACH(INPUT_FILE ${COMPILED_STD_LIB})
    get_filename_component(IN_FILENAME ${INPUT_FILE} NAME)
    SET(OUTPUT_FILE ${IN_FILENAME}.cc)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE}
        COMMAND ${RESOURCE_COMPILER} -i "std/${IN_FILENAME}" ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE}
        DEPENDS ${INPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Compiling ${INPUT_FILE} to binary")
    LIST(APPEND COMPILED_STD_LIB ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE})
ENDFOREACH()

add_library(exys STATIC interpreter.cc graph.cc parser.cc ${COMPILED_STD_LIB})

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

