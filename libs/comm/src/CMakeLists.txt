set(COMMLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/psk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/qam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/fsk_ofdm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/analog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/fading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/pulse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/scrambler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/errors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/compand.cpp
)

set(COMMLIB_SOURCES ${COMMLIB_SOURCES} PARENT_SCOPE)
