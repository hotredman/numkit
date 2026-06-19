// codegen/include/numkit/codegen/classinfo.hpp
//
// Class table (build plan §12, brick 4): parse a CLASSDEF_DEF into a
// ClassInfo (fields with inferred types, methods, value-vs-handle) and a
// ClassRegistry that hands out the classId carried by InferredType::object.
//
// v1 subset (DESIGN.md §7a): a value class (or a handle class via `<
// handle`), plain stored properties each with a concrete-typed default,
// and methods called monomorphically. Inheritance (beyond `handle`),
// property accessors (`get.`/`set.`), and Dependent/Constant/Abstract
// properties are REFUSED with a diagnostic — never miscompiled.

#pragma once

#include <numkit/codegen/transfer.hpp>
#include <numkit/codegen/type_lattice.hpp>

#include <numkit/core/ast.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::codegen {

struct ClassField {
    std::string    name;
    InferredType   type;                  // inferred from the default expression
    const ASTNode *defaultExpr = nullptr;  // the property's default value expression
};

struct ClassInfo {
    std::string                                      name;
    bool                                             isHandle = false;
    int                                              id       = -1;
    std::vector<ClassField>                          fields;
    std::unordered_map<std::string, const ASTNode *> methods;  // name -> FUNCTION_DEF
    const ASTNode                                   *def = nullptr;  // the CLASSDEF_DEF

    const ClassField *field(const std::string &n) const;
    const ASTNode    *method(const std::string &n) const;
};

// classId -> ClassInfo (id == insertion index). Borrows AST nodes (owned by
// the caller's parse tree, which must outlive the registry).
class ClassRegistry {
public:
    int              add(ClassInfo info);  // assigns + returns the classId
    const ClassInfo *byId(int id) const;
    const ClassInfo *byName(const std::string &name) const;
    int              idOf(const std::string &name) const;  // -1 if absent
    bool             has(const std::string &name) const { return idOf(name) >= 0; }
    std::size_t      size() const { return classes_.size(); }

private:
    std::vector<ClassInfo>               classes_;
    std::unordered_map<std::string, int> byName_;
};

// Why `classDef` is outside the v1 compilable subset; empty string =
// accepted. (Inheritance beyond `handle`, `get.`/`set.` accessors,
// Dependent/Constant/Abstract properties.)
std::string classRefusalReason(const ASTNode &classDef);

// Build a ClassInfo from a CLASSDEF_DEF: `id` is the classId it will hold;
// field types are inferred from property defaults under `reg`. Throws
// std::runtime_error when the class is refused (classRefusalReason) or a
// property lacks a concrete-typed default.
ClassInfo buildClassInfo(const ASTNode &classDef, int id, const TransferRegistry &reg);

// Collect every CLASSDEF_DEF reachable in `root` into `reg`. Throws on a
// refused class (loud, per §7a). Returns the number added.
std::size_t collectClasses(const ASTNode &root, ClassRegistry &reg,
                           const TransferRegistry &reg2);

} // namespace numkit::codegen
