# World renderer (land, statics) and GPU hueing for world and UI sprites.
#
#   uocore (uo/core/.../render)  engine-free model, sorting and geometry (tested in uo/tests)
#   uo_world_axmol               the Axmol node that draws it (built when axmol is a target)
#
# The app links uo_world_axmol and calls uo_world_add_shaders(<app>) after
# ax_setup_app_config so axslcc compiles uo_world_vs/fs into custom/.

set(UO_WORLD_SHADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/uo_world_vs.hlsl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/uo_world_fs.hlsl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/uo_hue_fs.hlsl
  CACHE INTERNAL "World renderer shader sources")

if(TARGET axmol)
  add_library(uo_world_axmol STATIC
    axmol/HueShader.cpp axmol/HueShader.h
    axmol/WorldRenderer.cpp axmol/WorldRenderer.h)
  target_link_libraries(uo_world_axmol PUBLIC uocore axmol)
endif()

function(uo_world_add_shaders app)
  # Not ax_add_shader_target_for: the app already owns <app>_shaders for its
  # own Source/Shaders directory.
  ax_add_shader_target(uo_world_shaders FILES ${UO_WORLD_SHADERS})
  ax_add_shader_dependencies(${app} uo_world_shaders)
endfunction()
