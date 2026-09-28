# uoconvert: offline UO data -> Axmol asset converter, built on uocore's format readers.

file(GLOB UOCONVERT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
list(FILTER UOCONVERT_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(uoconvert_lib STATIC ${UOCONVERT_SOURCES})
target_include_directories(uoconvert_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(uoconvert_lib SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(uoconvert_lib PUBLIC uocore)
# libvorbis (with vorbisenc) for Ogg music: Axmol vendors it in 3rdparty/ogg. Inside an Axmol build
# the engine's target is reused.
if(NOT TARGET ogg)
  set(AX_ENABLE_OPUS OFF)
  enable_language(C)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/ogg ${CMAKE_CURRENT_BINARY_DIR}/ogg EXCLUDE_FROM_ALL)
endif()
target_link_libraries(uoconvert_lib PRIVATE ogg)
find_package(Threads REQUIRED)
target_link_libraries(uoconvert_lib PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(uoconvert_lib PRIVATE /W4 /permissive-)
else()
  target_compile_options(uoconvert_lib PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(uoconvert ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(uoconvert PRIVATE uoconvert_lib)

if(UO_BUILD_TESTS)
  file(GLOB UOCONVERT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(uoconvert-tests ${UOCONVERT_TESTS})
  target_link_libraries(uoconvert-tests PRIVATE uoconvert_lib ogg)
  target_include_directories(uoconvert-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/doctest
                                                     ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
                                                     ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/stb)
  add_test(NAME uoconvert-tests COMMAND uoconvert-tests)
endif()
