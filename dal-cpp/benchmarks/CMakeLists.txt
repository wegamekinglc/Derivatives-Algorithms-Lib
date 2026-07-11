set(DAL_BENCHMARK_TARGETS
    matrix_perf
    script_perf
    tape_perf
    jacobian_perf
    pde_perf
    rng_perf
    interp_perf
    krylov_perf
    banded_perf
    cholesky_perf
    specialfunctions_perf
    black_perf
    iv_brent_perf
    script_mc_perf
    curve_calibration_perf
    threadpool_perf
    stacks_perf)

foreach(benchmark_target IN LISTS DAL_BENCHMARK_TARGETS)
    add_subdirectory(${benchmark_target})
    dal_apply_platform_options(${benchmark_target})
endforeach()
