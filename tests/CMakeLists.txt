# tests/CMakeLists.txt — dispatcher for the test-related binaries.
#
# Two siblings:
#   tests/gtest/   numkit_gtest binary — gtest unit-test suite. Per-component
#                  tests/CMakeLists.txt files attach their sources to it via
#                  target_sources(numkit_gtest PRIVATE ...).
#   tests/smoke/   numkit_smoke binary — runs *_smoke.m hand-runnable scripts.
#
# Performance benchmarks live separately under benchmarks/ (different
# mental model — perf vs correctness, different cadence and output).

add_subdirectory(gtest)
add_subdirectory(smoke)
