message(STATUS "Handler Module: DNS")

pkg_check_modules(LIBPCPP REQUIRED PcapPlusPlus)

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

corrade_add_static_plugin(DnsHandler ${CMAKE_CURRENT_BINARY_DIR}
        DnsHandler.conf
        DnsHandlerModulePlugin.cpp
        DnsStreamHandler.cpp
        dns.cpp
        querypairmgr.cpp
        # DnsLayer
        DnsLayer.cpp
        DnsResource.cpp
        DnsResourceData.cpp
        )

target_include_directories(DnsHandler
        PRIVATE
        ${PKTVISOR_VENDOR_DIR}
        # FIXME
        ${PKTVISOR_VENDOR_DIR}/datasketches
        ${PKTVISOR_VENDOR_DIR}/datasketches/datasketches
        #
        ${LIBPCPP_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src # FIXME ?
        ${CMAKE_SOURCE_DIR}/src/inputs/pcap # FIXME - dependency on pcap
        ${CORRADE_INCLUDE_DIR}
        ${Corrade_BINARY_DIR}/src # FIXME configure.h, probably not right
        )

if (APPLE)
    target_link_libraries(DnsHandler
            PRIVATE
            "-L${LIBPCPP_LIBRARY_DIRS}"
            ${LIBPCPP_LIBRARIES}
            "-framework CoreFoundation"
            "-framework SystemConfiguration"
            )
else ()
    target_link_libraries(DnsHandler
            PRIVATE
            ${LIBPCPP_LDFLAGS}
            ${LIBPCPP_LIBRARIES}
            )
endif ()

target_link_libraries(DnsHandler
        PRIVATE
        pktvisorcore
        )

set(PKTVISOR_STATIC_PLUGINS ${PKTVISOR_STATIC_PLUGINS} DnsHandler PARENT_SCOPE)

## TEST SUITE
add_executable(unit-tests-handler-dns
        tests/main.cpp
        tests/test_dns.cpp
        tests/test_dns_layer.cpp
        )

target_include_directories(unit-tests-handler-dns
        PRIVATE
        ${PKTVISOR_VENDOR_DIR}
        ${PKTVISOR_VENDOR_DIR}/catch
        ${CMAKE_CURRENT_SOURCE_DIR}
        #
        ${LIBPCPP_INCLUDE_DIRS}  # pcpp
        ${CMAKE_SOURCE_DIR}/src # core
        ${CMAKE_SOURCE_DIR}/src/inputs/pcap # PcapInputStream
        ${PKTVISOR_VENDOR_DIR}/datasketches
        ${PKTVISOR_VENDOR_DIR}/datasketches/datasketches
        )

target_link_libraries(unit-tests-handler-dns
        PRIVATE pktvisorcore PcapInput DnsHandler
        )

#if (APPLE)
#    target_link_libraries(unit-tests-handler-dns
#            PRIVATE
#            "-L${LIBPCPP_LIBRARY_DIRS}"
#            ${LIBPCPP_LIBRARIES}
#            "-framework CoreFoundation"
#            "-framework SystemConfiguration"
#            )
#else ()
#    target_link_libraries(unit-tests-handler-dns
#            PRIVATE
#            ${LIBPCPP_LDFLAGS}
#            ${LIBPCPP_LIBRARIES}
#            )
#endif ()