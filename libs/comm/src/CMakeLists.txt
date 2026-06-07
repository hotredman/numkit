set(COMMLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/psk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/qam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/fsk_ofdm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/analog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/generic_qam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/apsk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modulation/mil188.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/fading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/pulse.cpp                     # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/pulse_reg.cpp                # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/scrambler.cpp                # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/scrambler_reg.cpp           # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/errors.cpp                   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/errors_reg.cpp              # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/compand.cpp                  # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/eq/compand_reg.cpp             # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/random_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/huffman.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dpcm.cpp                   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dpcm_reg.cpp              # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/quantiz.cpp               # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/quantiz_reg.cpp           # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lloyds.cpp                # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lloyds_reg.cpp            # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dpcmopt.cpp               # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dpcmopt_reg.cpp           # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/arithcoding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/base_conversions.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/base_conversions_reg.cpp  # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/coding/convcoding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coding/blockcoding.cpp
)

set(COMMLIB_SOURCES ${COMMLIB_SOURCES} PARENT_SCOPE)
