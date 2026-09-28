# Movement and targeting input for the Axmol client, added by uo/client/CMakeLists.txt after the
# axmol and uocore targets exist and linked into the app.
add_library(uo_client_input STATIC InputRouter.cpp InputRouter.h)
target_include_directories(uo_client_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(uo_client_input PUBLIC uocore axmol)
target_compile_features(uo_client_input PUBLIC cxx_std_20)
