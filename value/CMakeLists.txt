# value/CMakeLists.txt
#
# numkit_value — the Value substrate. The MATLAB value/array model (Value,
# DataBuffer, Dims, ValueType, heap_object), the object-instance model
# (ObjectState + BuiltinClass — hook *shapes* only; CallContext is referenced
# by forward declaration, never included), N-D shape ops, the FnHandle
# callback type, and the ScratchArena.
#
# An L0 foundation: depends ONLY on the C++ standard library — never on the
# engine, parser, VM, fs, or any toolbox. `core` and the pure `toolboxes/*` build
# on it without pulling each other in. (Primitive element-wise operator
# kernels land here in a follow-up step; until then the VM still calls them
# from toolboxes/builtin.)

add_library(numkit_value STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dims.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/heap_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value.cpp
)

target_include_directories(numkit_value PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(numkit_value PUBLIC cxx_std_17)
set_target_properties(numkit_value PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EMSCRIPTEN)
    target_compile_options(numkit_value PRIVATE -fexceptions)
endif()
