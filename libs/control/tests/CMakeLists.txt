# libs/control/tests/CMakeLists.txt
#
# Control Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtests PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtests PRIVATE
    # Add per-function gtest TUs here.
)
