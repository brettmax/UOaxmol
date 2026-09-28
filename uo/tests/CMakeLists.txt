file(GLOB UOTEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(uocore-tests ${UOTEST_SOURCES})
target_link_libraries(uocore-tests PRIVATE uocore)
target_include_directories(uocore-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/doctest)
add_test(NAME uocore-tests COMMAND uocore-tests)
# Newer MSVC STL breaks doctest's forward declarations of std::ostream.
target_compile_definitions(uocore-tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
