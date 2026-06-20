// codegen/src/driver.cpp — see driver.hpp.

#include <numkit/codegen/driver.hpp>

#include <numkit/codegen/classinfo.hpp>
#include <numkit/codegen/monomorphize.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/ast.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace numkit::codegen::driver {

namespace {

std::string trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

ValueType dtypeFromString(const std::string &name)
{
    static const std::unordered_map<std::string, ValueType> kMap = {
        {"double", ValueType::DOUBLE},  {"single", ValueType::SINGLE},
        {"complex", ValueType::COMPLEX}, {"logical", ValueType::LOGICAL},
        {"int8", ValueType::INT8},      {"int16", ValueType::INT16},
        {"int32", ValueType::INT32},    {"int64", ValueType::INT64},
        {"uint8", ValueType::UINT8},    {"uint16", ValueType::UINT16},
        {"uint32", ValueType::UINT32},  {"uint64", ValueType::UINT64}};
    const auto it = kMap.find(name);
    if (it == kMap.end())
        throw std::runtime_error("numkit build: unknown argument type '" + name + "'");
    return it->second;
}

} // namespace

std::vector<InferredType> parseTypeSpec(const std::string &spec)
{
    std::vector<InferredType> out;
    const std::string         s = trim(spec);
    if (s.empty()) return out;  // nullary entry

    std::size_t pos = 0;
    while (pos <= s.size()) {
        const std::size_t comma = s.find(',', pos);
        std::string       tok   = trim(s.substr(pos, comma == std::string::npos
                                                         ? std::string::npos
                                                         : comma - pos));
        if (tok.empty())
            throw std::runtime_error("numkit build: empty argument type in spec");

        bool isArray = false;
        if (tok.size() >= 2 && tok.substr(tok.size() - 2) == "[]") {
            isArray = true;
            tok     = trim(tok.substr(0, tok.size() - 2));
        }
        const ValueType dt = dtypeFromString(tok);
        out.push_back(isArray ? InferredType::concrete(dt, Shape::rowVector())
                              : InferredType::scalar(dt));

        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

EmittedFunction transpileSource(const std::string &source, const std::string &entry,
                                const std::vector<InferredType> &paramTypes,
                                const BridgeOptions &bridge)
{
    Lexer  lex(source);
    Parser parser(lex.tokenize());
    auto   root = parser.parse();
    if (!root) throw std::runtime_error("numkit build: failed to parse source");

    // Register transfers + user functions + classes so calls / methods type.
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry classes;
    collectClasses(*root, classes, reg);
    FunctionTable table;
    collectFunctions(*root, table);
    registerUserFunctions(reg, table);
    if (classes.size() > 0) {
        registerClassMethods(reg, classes);
        registerClassConstructors(reg, classes);
    }
    const ClassRegistry *classesPtr = classes.size() > 0 ? &classes : nullptr;

    // Resolve the entry: the named function, or the sole one.
    const ASTNode *fn = nullptr;
    if (!entry.empty()) {
        fn = table.find(entry);
        if (!fn) throw std::runtime_error("numkit build: no function named '" + entry + "'");
    } else if (table.size() == 1) {
        fn = table.entries().begin()->second;
    } else {
        throw std::runtime_error(
            "numkit build: source has " + std::to_string(table.size())
            + " functions — specify the entry with --entry");
    }

    if (paramTypes.size() != fn->paramNames.size())
        throw std::runtime_error("numkit build: entry '" + fn->strValue + "' takes "
                                 + std::to_string(fn->paramNames.size())
                                 + " argument(s) but the type spec has "
                                 + std::to_string(paramTypes.size()));

    std::vector<ParamSpec> params;
    params.reserve(paramTypes.size());
    for (std::size_t i = 0; i < paramTypes.size(); ++i)
        params.push_back({fn->paramNames[i], paramTypes[i]});

    return emitProgram(*fn, params, table, reg, classesPtr, bridge);
}

} // namespace numkit::codegen::driver
