message(STATUS "Input Module: Mock")

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

corrade_add_static_plugin(VisorInputMock ${CMAKE_CURRENT_BINARY_DIR}
        Mock.conf
        MockInputModulePlugin.cpp
        MockInputStream.cpp
        )
add_library(Visor::Input::Mock ALIAS VisorInputMock)

target_include_directories(VisorInputMock
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        )

target_link_libraries(VisorInputMock
        PUBLIC
        Visor::Core
        )

set(VISOR_STATIC_PLUGINS ${VISOR_STATIC_PLUGINS} Visor::Input::Mock PARENT_SCOPE)

## TEST SUITE
add_executable(unit-tests-input-mock
        tests/main.cpp
        tests/test_mock.cpp
        )

target_link_libraries(unit-tests-input-mock
        PRIVATE Visor::Input::Mock
        )

add_test(NAME unit-tests-input-mock
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
        COMMAND unit-tests-input-mock
        )
