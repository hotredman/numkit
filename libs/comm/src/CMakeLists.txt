set(COMMLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/psk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/qam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/fsk_ofdm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/channel.cpp
)

set(COMMLIB_SOURCES ${COMMLIB_SOURCES} PARENT_SCOPE)
