# toolboxes/comm/CMakeLists.txt
#
# Communications Toolbox. Depends on builtin + signal (FFT for OFDM).

add_subdirectory(src)  # exports COMMLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${COMMLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/channel/channel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/channel/fading.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/eq/pulse.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/eq/scrambler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/modulation/fsk_ofdm.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/modulation/psk.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/comm/modulation/qam.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
