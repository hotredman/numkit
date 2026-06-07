# libs/linalg/src/CMakeLists.txt
#
# Linear Algebra Toolbox sources. Files are added per migration group
# as functions move out of libs/builtin/src/language/arrays/.

set(LINALGLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_ops.cpp          # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_ops_reg.cpp      # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/norms.cpp                # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/norms_reg.cpp            # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/properties.cpp          # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/properties_reg.cpp      # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/decompositions.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/decompositions_reg.cpp  # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/pseudo_subspace.cpp     # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/pseudo_subspace_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/balance.cpp             # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/balance_reg.cpp         # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/ldl.cpp                 # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/ldl_reg.cpp             # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/eig.cpp                  # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/eig_reg.cpp             # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/matrix_functions.cpp     # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/matrix_functions_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/predicates.cpp          # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/predicates_reg.cpp      # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/solvers.cpp             # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/solvers_reg.cpp         # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/misc.cpp                # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/misc_reg.cpp            # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/page_ops.cpp            # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/page_ops_reg.cpp        # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/schur_convert.cpp       # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/schur_convert_reg.cpp   # register (CallContext)
)

set(LINALGLIB_SOURCES ${LINALGLIB_SOURCES} PARENT_SCOPE)
