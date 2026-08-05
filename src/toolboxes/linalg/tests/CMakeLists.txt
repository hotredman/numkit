# toolboxes/linalg/tests/CMakeLists.txt
#
# Gtest unit tests for toolboxes/linalg. Each *_test.cpp is appended to
# the global numkit_gtest target. Smoke .m files in tests/smoke/ are
# hand-runnable demos for numkit (not built into gtest).

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/condeig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/condest_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cholupdate_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qr_update_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/page_family_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/schur_convert_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/polyeig_ordeig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/expmv_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/funm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cond_pnorm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/norm_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qr_pivoting_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eig_left_vectors_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kron_integer_class_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cross_integer_class_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/page_lsq_mrdiv_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lu_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qr_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/chol_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/schur_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/svd_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matrix_functions_general_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ordschur_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qz_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gsvd_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/balance_perm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eigs_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/funm_parlett_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)

# Relocated from tests/builtin (layer split 2026-06-17).
target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/balance_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/colperm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hess_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inv_linsolve_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ldl_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lsq_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/luqr_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matchpairs_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matfunc_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mldivide_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/norm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/predicates_linalg_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rref_rcond_planerot_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/subspace_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/svd_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/svd_deps_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sylvester_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symrcm_test.cpp
)
