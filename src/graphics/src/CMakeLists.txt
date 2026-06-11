set(GRAPHICSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp  # Engine-coupled installer + adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/plots.cpp    # core-free plotting bodies (buildPlotTable)
    PARENT_SCOPE
)
