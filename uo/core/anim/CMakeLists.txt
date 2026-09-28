# uocore_anim: mobile, mount and equipment animations (ClassicUO AnimationsLoader and the
# engine-free half of its animation renderer). zlib (for UOP animation frames) comes from
# uocore's public link.
file(GLOB UOCORE_ANIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB UOCORE_ANIM_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/uo/anim/*.h)

add_library(uocore_anim STATIC ${UOCORE_ANIM_SOURCES} ${UOCORE_ANIM_HEADERS})
target_include_directories(uocore_anim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(uocore_anim PUBLIC uocore)
target_compile_features(uocore_anim PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(uocore_anim PRIVATE /W4 /permissive-)
else()
  target_compile_options(uocore_anim PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
