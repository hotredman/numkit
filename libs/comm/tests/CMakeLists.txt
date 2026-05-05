# libs/comm/tests/CMakeLists.txt
#
# Communications Toolbox unit tests. Each new function in /loop adds a
# TU here via target_sources(numkit_tests PRIVATE ...). Hand-runnable
# .m smokes live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_tests PRIVATE
    # Add per-function gtest TUs here.
)
