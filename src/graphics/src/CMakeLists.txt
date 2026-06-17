set(GRAPHICSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/plots.cpp                # buildPlotTable dispatcher + bodies
    ${CMAKE_CURRENT_SOURCE_DIR}/plots/helpers.cpp        # shared JSON/parse helpers (numkit::detail)
    PARENT_SCOPE
)
