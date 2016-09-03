add_executable(bench bench_interpreter_vs_jit.cc)

target_link_libraries(bench exys benchmark)
