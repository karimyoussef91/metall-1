include(setup_omp)

set(BUILD_TESTING OFF)
FetchContent_Declare(faker
    GIT_REPOSITORY https://github.com/cieslarmichal/faker-cxx.git
    GIT_TAG main
)
FetchContent_MakeAvailable(faker)

add_metall_executable(ingest_str_bench ingest_str_bench.cpp)
setup_omp_target(ingest_str_bench)
target_link_libraries(ingest_str_bench PUBLIC faker-cxx)

find_package(MPI REQUIRED)

add_metall_executable(ingest_str_bench_mpi ingest_str_bench_mpi.cpp)
setup_omp_target(ingest_str_bench_mpi)
target_link_libraries(ingest_str_bench_mpi PUBLIC faker-cxx)
target_link_libraries(ingest_str_bench_mpi PUBLIC MPI::MPI_CXX)