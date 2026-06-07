set(CONTROLLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/numerics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lti/lti.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/conversion/conversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/props/props.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connect/connect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/response/response.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/freq/freq.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/analyze/analyze.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/discretize/discretize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/state/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/state/place.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lyapunov/lyapunov.cpp       # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/lyapunov/lyapunov_reg.cpp   # register (CallContext)
)

set(CONTROLLIB_SOURCES ${CONTROLLIB_SOURCES} PARENT_SCOPE)
