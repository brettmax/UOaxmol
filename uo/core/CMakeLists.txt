# AxmolUO core: UO file formats, protocol and game state. Engine-independent C++20, so it
# builds and tests without Axmol and can be shared with tools and a headless bot/test client.
cmake_minimum_required(VERSION 3.22)

file(GLOB_RECURSE UOCORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE UOCORE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(uocore STATIC ${UOCORE_SOURCES} ${UOCORE_HEADERS})
target_include_directories(uocore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(uocore PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(uocore PRIVATE /W4 /permissive-)
else()
  target_compile_options(uocore PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# zlib: UOP entries (gumps, 0xDD gump packets, 0xD8 custom houses) are deflated. Inside an
# Axmol build use the engine's bundled zlib; standalone, the system one.
if(TARGET zlib)
  target_link_libraries(uocore PUBLIC zlib)
else()
  find_package(ZLIB REQUIRED)
  target_link_libraries(uocore PUBLIC ZLIB::ZLIB)
endif()

add_subdirectory(anim)
