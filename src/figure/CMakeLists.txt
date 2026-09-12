# Header-only figure-state layer (L0.5).
#
# FigureManager — the plot/figure session state (figures, axes, datasets-as-JSON)
# with an OutputFunc callback for emitting to the host. Moved out of core/ so the
# graphics service can depend on figure state WITHOUT the Engine. Uses only
# ops (decimate) + STL; never core. The Engine owns one FigureManager and wires
# its OutputFunc to the engine output sink.
list(APPEND NK_FIGURE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NK_FIGURE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/figure/figure_manager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/figure/figure_render.hpp)
set(NK_FIGURE_INCLUDE_DIRS ${NK_FIGURE_INCLUDE_DIRS} PARENT_SCOPE)
set(NK_FIGURE_HEADERS      ${NK_FIGURE_HEADERS}      PARENT_SCOPE)
