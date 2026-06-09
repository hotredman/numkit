set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # Tiny isolated TU owning the build-timestamp string. Forced to
    # recompile on every build by the `numkit_build_info` target in
    # the top-level CMakeLists, so the `version` builtin always
    # reflects the actual link time instead of stale __DATE__.
    ${CMAKE_CURRENT_SOURCE_DIR}/version_string.cpp
    # language/ — MATLAB Language Fundamentals
    # commands/env moved to runtime (runtime/src/language/commands/) — Phase 3-A step C3
    # operators/ (binary_ops, unary_ops) moved to the lang library (lang/src/operators/) — Phase 3-A step C2
    # la_solve.cpp moved to ops/ (numkit_ops) — shared linear-solve kernel
    # used by builtin `\` and toolboxes/linalg; ops/ sits below both.
    # arrays/ (matrix, manip, nd_manip) moved to the lang library (lang/src/arrays/) — Phase 3-A step C2
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/accum.cpp
    # predicates.cpp / linalg_extras.cpp / ldl.cpp / balance.cpp /
    # lsq.cpp all migrated to toolboxes/linalg.
    # types/ (types + casts SIMD) moved to the lang library (lang/src/types/) — Phase 3-A step C2
    # strings/ (strings/regex/format/print/scan) moved to the lang library (lang/src/strings/) — Phase 3-A step C2
    ${CMAKE_CURRENT_SOURCE_DIR}/language/cells/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/structures/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/datatypes/containers.cpp
    # bitwise/ (int_math) moved to the lang library (lang/src/bitwise/) — Phase 3-A step C2
    # math/ — MATLAB Mathematics
    # random/ (rng.cpp / rng_reg.cpp) moved to the math library (math/src/random/) — Phase 3-A step C1
    # matlab_mt19937.cpp moved to ops/ (numkit_ops) — the MT19937 engine is a
    # shared primitive; builtin's rng.cpp uses it via numkit::ops.
    # complex/ moved to the math library (math/src/complex/) — Phase 3-A step C1
    # poly/ moved to the math library (math/src/poly/) — Phase 3-A step C1
    # discrete/ moved to the math library (math/src/discrete/) — Phase 3-A step C1
    # trig/ moved to the math library (math/src/trig/) — Phase 3-A step C1
    # exp_log + special moved to the math library (math/src/) — Phase 3-A step C1
    # arithmetic/ (misc/reductions/rounding) moved to the math library (math/src/arithmetic/) — Phase 3-A step C1
    # interp/ moved to the math library (math/src/interp/) — Phase 3-A step C1
    ${CMAKE_CURRENT_SOURCE_DIR}/math/integration/integration.cpp
    # geom/ moved to the math library (math/src/geom/) — Phase 3-A step C1
    ${CMAKE_CURRENT_SOURCE_DIR}/math/group/group.cpp
    # permutations moved to the math library (math/src/permutations.cpp) — Phase 3-A step C1
    # math/optim/ moved to toolboxes/optim/
    # programming/ — MATLAB Programming and Scripts
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics_reg.cpp # register (CallContext)
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. SIMD and portable variants live next to
# their public-API consumers in the same feature directory:
#   math/arithmetic/  — abs, cumsum, logical reductions, var reduction
#   math/trig/        — sin, cos (trig family)
#   math/exp_log/     — exp, log (exp/log family)
#   language/operators/backends/ — binary ops, compare
if(NUMKIT_WITH_SIMD)
    # arithmetic SIMD backends moved to the math library (math/src/arithmetic/) — Phase 3-A step C1
    # trig SIMD backends moved to the math library (math/src/trig/) — Phase 3-A step C1
    # exp_log + special SIMD backends moved to the math library — Phase 3-A step C1
    # (binary_ops_highway.cpp + compare_highway.cpp moved to ops/ → numkit_ops)
    # casts SIMD backend (highway) moved to the lang library (lang/src/types/) — Phase 3-A step C2
else()
    # arithmetic SIMD backends moved to the math library (math/src/arithmetic/) — Phase 3-A step C1
    # trig SIMD backends moved to the math library (math/src/trig/) — Phase 3-A step C1
    # exp_log + special SIMD backends moved to the math library — Phase 3-A step C1
    # (binary_ops_portable.cpp + compare_portable.cpp moved to ops/ → numkit_ops)
    # casts SIMD backend (portable) moved to the lang library (lang/src/types/) — Phase 3-A step C2
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
