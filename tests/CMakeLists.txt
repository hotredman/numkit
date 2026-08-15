# tests/CMakeLists.txt — dispatcher for the test-related binaries.
#
# One sibling:
#   tests/gtest/   numkit_gtest binary — gtest unit-test suite. Per-component
#                  tests/CMakeLists.txt files attach their sources to it via
#                  target_sources(numkit_gtest PRIVATE ...).
#
# The `numkit` interpreter CLI (which runs the hand-runnable *_smoke.m demos)
# lives in apps/numkit/, not here — it is a product, not a test. Performance
# benchmarks live separately under benchmarks/ (different mental model — perf
# vs correctness, different cadence and output).

add_subdirectory(gtest)
