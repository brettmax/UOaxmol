# Text rendering for the Axmol client: UO-font labels, gump HTML text, overhead speech and
# the journal. Layout and pixels come from uocore's text area (tested in uo/tests); this
# library only turns them into textures and nodes.
#
# The app links uo_text_axmol (built when axmol is a target).

if(TARGET axmol)
  add_library(uo_text_axmol STATIC
    axmol/TextSystem.cpp axmol/TextSystem.h
    axmol/TextFactory.cpp axmol/TextFactory.h
    axmol/OverheadTextLayer.cpp axmol/OverheadTextLayer.h
    axmol/JournalView.cpp axmol/JournalView.h)
  target_include_directories(uo_text_axmol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(uo_text_axmol PUBLIC uocore axmol)
endif()
