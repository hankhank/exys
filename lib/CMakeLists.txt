add_library(exys STATIC interpreter.cc graph.cc parser.cc)

target_include_directories (exys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
