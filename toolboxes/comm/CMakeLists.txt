# toolboxes/comm/CMakeLists.txt
#
# Communications Toolbox. Depends on builtin + signal (FFT for OFDM).

add_subdirectory(src)  # exports COMMLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${COMMLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/channel/channel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/channel/fading.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/eq/pulse.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/eq/scrambler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/modulation/fsk_ofdm.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/modulation/psk.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/comm/modulation/qam.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
