
# Unit
add_executable(unit-tests-handler-dns
        main.cpp
        test_dns.cpp
        test_dns_layer.cpp
        test_json_schema.cpp
        )

target_link_libraries(unit-tests-handler-dns
        PRIVATE
        ${CONAN_LIBS_JSON-SCHEMA-VALIDATOR}
        Visor::Handler::Dns)

add_test(NAME unit-tests-handler-dns
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
        COMMAND unit-tests-handler-dns
        )

# Benchmark
add_executable(benchmark-handler-dns
        benchmark_dns.cpp
        )

target_link_libraries(benchmark-handler-dns PRIVATE
        Visor::Handler::Dns
        ${CONAN_LIBS_BENCHMARK})
