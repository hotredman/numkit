// codegen/include/numkit/codegen/driver.hpp
//
// The `numkit build` driver core (DESIGN.md §8 M4): the orchestration that
// turns a numkit source file into a native artifact — parse -> infer/emit
// (emitProgram) -> AOT-compile. The reusable, testable pieces live here; the
// CLI (tools/numkit_codegen_main.cpp) is a thin argv wrapper over them.

#pragma once

#include <numkit/codegen/emitter.hpp>
#include <numkit/codegen/type_lattice.hpp>

#include <string>
#include <vector>

namespace numkit::codegen::driver {

// Parse a CLI argument-type spec into entry parameter types (MATLAB-Coder
// `-args` style). Comma-separated tokens; each is a dtype optionally followed
// by `[]` for a row vector:
//   "double"            -> scalar double
//   "double[], double"  -> [row-vector double, scalar double]   (e.g. biquad's x)
// dtype is one of double/single/complex/logical/int8..int64/uint8..uint64.
// An empty spec yields no parameters (a nullary entry). Throws
// std::runtime_error on a malformed token.
std::vector<InferredType> parseTypeSpec(const std::string &spec);

// Transpile the entry function of `source` to a C++ TU (the emit step of
// `numkit build`). `entry` empty -> the sole top-level function (error if
// there is not exactly one). `paramTypes` (from parseTypeSpec) must match the
// entry's arity. Bridges uncompiled builtins when `bridge.enabled`. Throws
// std::runtime_error on parse / arity / unsupported-construct errors.
EmittedFunction transpileSource(const std::string &source, const std::string &entry,
                                const std::vector<InferredType> &paramTypes,
                                const BridgeOptions &bridge = {});

// Transpile the entry as a loadable numkit PLUGIN translation unit
// (codegen-as-plugin / tiered acceleration, DESIGN.md §6b): the compiled
// function + the nk_plugin_register hooks, ready to compile to a shared library
// and load with nk_load_plugin. `exportName` is the name the function registers
// under (callable from any numkit session); `abiHeaderPath` is the include path
// to nk_plugin.h the TU will #include. Requires a SCALAR-output function whose
// params are scalars / double vectors (emitScalarPlugin's contract); throws
// std::runtime_error otherwise. Returns the TU source.
std::string transpileToPlugin(const std::string &source, const std::string &entry,
                              const std::vector<InferredType> &paramTypes,
                              const std::string &exportName,
                              const std::string &abiHeaderPath);

} // namespace numkit::codegen::driver
