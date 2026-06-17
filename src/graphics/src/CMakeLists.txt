set(GRAPHICSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/plots.cpp                # buildPlotTable dispatcher + bodies
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/helpers.cpp        # shared JSON/parse helpers (numkit::detail)
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/contour.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/polar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/shared.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/line.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/bar.cpp
    PARENT_SCOPE
)
