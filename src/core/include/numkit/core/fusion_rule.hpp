// core/include/numkit/core/fusion_rule.hpp
//
// Element-wise fusion rule registry — the GENERIC, domain-free half of VM
// element-wise fusion. The engine owns a list of FusionRule; the standard
// library (bundle) registers concrete rules (idiom → ops kernel). The engine
// and both backends iterate the rules without knowing any specific idiom, so
// adding a fused idiom later touches only the kernel (ops/) and its rule
// (bundle/) — never core.
//
// Each rule is two closures:
//   * match    — pure AST inspection: is this node the idiom? If so, return the
//                operand sub-expressions (in the order `execute` expects). The
//                compiler / TreeWalker evaluate those operands themselves.
//   * execute  — runtime fast path: given the evaluated operand Values, fill
//                `out` and return true iff the inputs fit the kernel (real
//                double, conformable, large enough). Return false to fall back
//                to the ordinary per-op path. The fast path is computed
//                bit-identically to that path, so fused == unfused.
//
// The `execute` closure is where the type/shape guard and the ops-kernel call
// live, so core needs no knowledge of kernel signatures (the bundle captures
// them). This is what makes new idioms purely additive — see the FUSE_EWISE
// opcode / TreeWalker try-path consumers.

#pragma once

#include <cstddef>
#include <functional>
#include <memory_resource>
#include <optional>
#include <vector>

namespace numkit {

struct ASTNode;
class Value;

struct FusionRule {
    const char *name = "";

    // AST match → operand sub-expressions (executor order), or nullopt.
    std::function<std::optional<std::vector<const ASTNode *>>(const ASTNode *)>
        match;

    // Evaluated operands → fused result. true = fused (out filled);
    // false = guard failed, caller must fall back to the per-op path.
    std::function<bool(const Value *operands, std::size_t nOperands, Value &out,
                       std::pmr::memory_resource *mr)>
        execute;
};

} // namespace numkit
