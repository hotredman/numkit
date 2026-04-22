target_sources(m_tests PRIVATE
    # lexer / parser / compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_syntax_test.cpp
    # TreeWalker / VM backends
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_walker_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_benchmark.cpp
    # Thread pool — no-op file body when NUMKIT_WITH_THREADS is OFF
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_pool_test.cpp
)
