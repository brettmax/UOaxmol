# /****************************************************************************
# Copyright (c) 2013-2014 cocos2d-x.org
# Copyright (c) 2019-present Simdsoft Limited.
#
# https://axmol.dev/
#
# SPDX-License-Identifier: MIT
# ****************************************************************************/

cmake_minimum_required(VERSION 3.22...4.1)

set(APP_NAME AxmolUO)

project(${APP_NAME})

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

set(_is_axmol_embed FALSE)
set(_AX_USE_PREBUILT FALSE)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# NOTE: The order of the cmake module "include(AXGame...)" statements matters
# In-tree default: this project lives at <engine>/uo/client, so the engine root is two levels
# up unless AX_ROOT says otherwise.
if("$ENV{AX_ROOT}" STREQUAL "")
  get_filename_component(_uo_engine_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
  set(ENV{AX_ROOT} "${_uo_engine_root}")
endif()

include(AXGameEngineOptions)
include(AXGameEngineSetup)

# The engine-independent UO core (file formats, protocol, game state).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_BINARY_DIR}/uocore)

# The common cross-platforms source files and header files
file(GLOB_RECURSE GAME_HEADER
  Source/*.h Source/*.hpp
)

file(GLOB_RECURSE GAME_SOURCE
  Source/*.cpp Source/*.c
)

set(GAME_INC_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

set(content_folder
  "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

if(APPLE)
  ax_mark_multi_resources(common_content_files RES_TO "Resources" FOLDERS ${content_folder})
elseif(WINDOWS)
  ax_mark_multi_resources(common_content_files RES_TO "Content" FOLDERS ${content_folder})
endif()

include(AXGameSourceSetup)

# mark app complie info and libs info
set(APP_SOURCES
  ${GAME_HEADER}
  ${GAME_SOURCE}
)

include(AXGameTargetSetup)

# mark app resources, resource will be copy auto after mark
ax_setup_app_config(${APP_NAME})

# Add any libraries you need to link to the project after this point
target_link_libraries(${APP_NAME} uocore)

# Mouse and keyboard movement and targeting (uo/client/input).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/input ${CMAKE_BINARY_DIR}/uo_client_input)
target_link_libraries(${APP_NAME} uo_client_input)

# Batched world renderer and GPU hue shader (uo/client/world).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/world ${CMAKE_BINARY_DIR}/uo_world)
target_link_libraries(${APP_NAME} uo_world_axmol)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
uo_world_add_shaders(${APP_NAME})

# UO fonts (fonts.mul, unifont), cliloc text, overhead text and journal (uo/client/text).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/text ${CMAKE_BINARY_DIR}/uo_text)
target_link_libraries(${APP_NAME} uo_text_axmol)

# Sound effects and music on AudioEngine (uo/client/audio).
include(${CMAKE_CURRENT_SOURCE_DIR}/audio/sources.cmake)
target_sources(${APP_NAME} PRIVATE ${UO_CLIENT_AUDIO_SOURCES})
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Default Platform-specific setup
include(AXGamePlatformSetup)

# Make sure the AXGameFinalSetup is included at the end of this file
include(AXGameFinalSetup)
