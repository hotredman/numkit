# Header-only figure-state layer (L0.5).
#
# FigureManager — the plot/figure session state (figures, axes, datasets-as-JSON)
# with an OutputFunc callback for emitting to the host. Moved out of core/ so the
# graphics service can depend on figure state WITHOUT the Engine. Uses only
# ops (decimate) + STL; never core. The Engine owns one FigureManager and wires
# its OutputFunc to the engine output sink.
list(APPEND NUMKIT_FIGURE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_FIGURE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/figure/figure_manager.hpp)
set(NUMKIT_FIGURE_INCLUDE_DIRS ${NUMKIT_FIGURE_INCLUDE_DIRS} PARENT_SCOPE)
set(NUMKIT_FIGURE_HEADERS      ${NUMKIT_FIGURE_HEADERS}      PARENT_SCOPE)
