# cmake/Doxygen.cmake
#
# Adds `docs` (HTML) and `docs-pdf` (LaTeX/PDF) targets.
#
# Both targets are optional — they only appear if Doxygen is on PATH.
# `docs-pdf` additionally requires pdflatex (MiKTeX / TeX Live).
# Graphviz / dot is auto-detected; class & include graphs are enabled
# in the Doxyfile when found.
#
# Outputs:
#   build/<preset>/docs/html/index.html
#   build/<preset>/docs/latex/refman.pdf
#   build/<preset>/docs/doxygen-warnings.log
#
# Usage from the top-level CMakeLists.txt:
#   include(cmake/Doxygen.cmake)
#
# Then from the build directory:
#   cmake --build . --target docs        # HTML
#   cmake --build . --target docs-pdf    # PDF (extends docs first)
#   cmake --build . --target docs-clean  # wipe build/docs

find_package(Doxygen QUIET COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found — `docs` target unavailable. "
                   "Install via: winget install DimitriVanHeesch.Doxygen "
                   "(Windows) or your package manager.")
    return()
endif()

message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE} (${DOXYGEN_VERSION})")

# Graphviz detection (DOXYGEN_DOT_FOUND set by find_package above).
if(DOXYGEN_DOT_FOUND)
    message(STATUS "Graphviz dot found: ${DOXYGEN_DOT_EXECUTABLE} — "
                   "class / include diagrams enabled")
    set(_NUMKIT_DOC_HAVE_DOT "YES")
else()
    message(STATUS "Graphviz dot NOT found — text-only graphs in docs")
    set(_NUMKIT_DOC_HAVE_DOT "NO")
endif()

# Source-tree Doxyfile drives the build; we patch HAVE_DOT and the
# OUTPUT_DIRECTORY at configure time so each build preset gets its own
# docs/ tree inside build/<preset>/.
set(_NUMKIT_DOXYFILE_IN  "${CMAKE_SOURCE_DIR}/Doxyfile")
set(_NUMKIT_DOXYFILE_OUT "${CMAKE_BINARY_DIR}/Doxyfile.configured")
set(_NUMKIT_DOC_OUTPUT   "${CMAKE_BINARY_DIR}/docs")

# Read the template, override the two fields we care about per-build.
file(READ "${_NUMKIT_DOXYFILE_IN}" _NUMKIT_DOXYFILE_CONTENT)

string(REGEX REPLACE
    "OUTPUT_DIRECTORY[ ]*=[ ]*[^\n]*"
    "OUTPUT_DIRECTORY       = ${_NUMKIT_DOC_OUTPUT}"
    _NUMKIT_DOXYFILE_CONTENT "${_NUMKIT_DOXYFILE_CONTENT}")

string(REGEX REPLACE
    "HAVE_DOT[ ]*=[ ]*[^\n]*"
    "HAVE_DOT               = ${_NUMKIT_DOC_HAVE_DOT}"
    _NUMKIT_DOXYFILE_CONTENT "${_NUMKIT_DOXYFILE_CONTENT}")

# WARN_LOGFILE is relative-to-cwd by default; pin it to the same docs
# tree so warnings stay alongside the generated HTML / PDF.
string(REGEX REPLACE
    "WARN_LOGFILE[ ]*=[ ]*[^\n]*"
    "WARN_LOGFILE           = ${_NUMKIT_DOC_OUTPUT}/doxygen-warnings.log"
    _NUMKIT_DOXYFILE_CONTENT "${_NUMKIT_DOXYFILE_CONTENT}")

file(WRITE "${_NUMKIT_DOXYFILE_OUT}" "${_NUMKIT_DOXYFILE_CONTENT}")

# Plain HTML target — fast, no LaTeX dependency.
add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_NUMKIT_DOC_OUTPUT}"
    COMMAND ${DOXYGEN_EXECUTABLE} "${_NUMKIT_DOXYFILE_OUT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating HTML API documentation → ${_NUMKIT_DOC_OUTPUT}/html/index.html"
    VERBATIM
)

# PDF target — depends on docs (HTML pass also emits latex/) and runs
# `make` (or equivalent) inside the LaTeX output dir.
find_program(_NUMKIT_PDFLATEX pdflatex)
if(_NUMKIT_PDFLATEX)
    message(STATUS "pdflatex found: ${_NUMKIT_PDFLATEX} — `docs-pdf` target available")

    # The pdflatex command frequently exits non-zero on harmless
    # warnings even when refman.pdf is produced successfully — wrap
    # each invocation in a CMake script so we can ignore the exit
    # code and surface a clean error only if the PDF doesn't exist.
    set(_NUMKIT_PDFBUILD_SCRIPT "${CMAKE_BINARY_DIR}/_numkit_build_pdf.cmake")
    file(WRITE "${_NUMKIT_PDFBUILD_SCRIPT}" "
        # Two pdflatex passes for cross-reference stability.
        set(LATEX_DIR \"${_NUMKIT_DOC_OUTPUT}/latex\")
        if(NOT EXISTS \"\${LATEX_DIR}/refman.tex\")
            message(FATAL_ERROR \"refman.tex not found — run `docs` first.\")
        endif()
        foreach(_pass 1 2)
            execute_process(
                COMMAND \"${_NUMKIT_PDFLATEX}\" -interaction=batchmode refman.tex
                WORKING_DIRECTORY \"\${LATEX_DIR}\"
                RESULT_VARIABLE _rc
                OUTPUT_QUIET
                ERROR_QUIET
            )
            message(STATUS \"pdflatex pass \${_pass}: exit=\${_rc}\")
        endforeach()
        if(NOT EXISTS \"\${LATEX_DIR}/refman.pdf\")
            message(FATAL_ERROR \"refman.pdf was not produced — see \${LATEX_DIR}/refman.log\")
        endif()
        message(STATUS \"PDF reference: \${LATEX_DIR}/refman.pdf\")
    ")

    add_custom_target(docs-pdf
        DEPENDS docs
        COMMAND ${CMAKE_COMMAND} -P "${_NUMKIT_PDFBUILD_SCRIPT}"
        COMMENT "Building PDF reference → ${_NUMKIT_DOC_OUTPUT}/latex/refman.pdf"
        VERBATIM
    )
else()
    message(STATUS "pdflatex NOT found — `docs-pdf` target unavailable "
                   "(install MiKTeX / TeX Live to enable)")
endif()

# Convenience: wipe the docs output (without rebuilding the rest).
add_custom_target(docs-clean
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_NUMKIT_DOC_OUTPUT}"
    COMMENT "Removing ${_NUMKIT_DOC_OUTPUT}"
    VERBATIM
)
