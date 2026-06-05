# libs/linalg/tests/CMakeLists.txt
#
# Gtest unit tests for libs/linalg. Each *_test.cpp is appended to
# the global numkit_gtest target. Smoke .m files in tests/smoke/ are
# hand-runnable demos for numkit_smoke (not built into gtest).

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/condeig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/condest_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cholupdate_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qr_update_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/page_family_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/schur_convert_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/polyeig_ordeig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/expmv_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cond_pnorm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/norm_complex_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qr_pivoting_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eig_left_vectors_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kron_integer_class_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cross_integer_class_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)
