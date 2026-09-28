# SPDX-License-Identifier: BSD-2-Clause
#
# include() from the client app's CMakeLists and add ${UO_CLIENT_AUDIO_SOURCES} to its sources.
# Links against uocore and axmol.

set(UO_CLIENT_AUDIO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/AudioManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AudioManager.h
)
