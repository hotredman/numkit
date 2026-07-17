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

namespace {

// Parse `source`, register transfers/user-functions/classes, resolve the entry
// (named or sole), arity-check, build the ParamSpec list, then invoke `emit`
// WHILE everything is still alive. Nothing borrowing escapes — the registry's
// user-function transfers reference these locals, so emission must run inside
// this scope (shared by both transpileSource and transpileToPlugin).
template <class Emit>
auto withPrepared(const std::string &source, const std::string &entry,
                  const std::vector<InferredType> &paramTypes, Emit &&emit)
{
    Lexer  lex(source);
    Parser parser(lex.tokenize());
    auto   root = parser.parse();
    if (!root) throw std::runtime_error("numkit build: failed to parse source");

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

    const ASTNode *fn = nullptr;
    if (!entry.empty()) {
        fn = table.find(entry);
        if (!fn) throw std::runtime_error("numkit build: no function named '" + entry + "'");
    } else if (table.size() == 1) {
        fn = table.entries().begin()->second;
    } else {
        throw std::runtime_error("numkit build: source has " + std::to_string(table.size())
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

    return emit(*fn, params, reg, table, classesPtr);
}

} // namespace

EmittedFunction transpileSource(const std::string &source, const std::string &entry,
                                const std::vector<InferredType> &paramTypes,
                                const BridgeOptions &bridge)
{
    return withPrepared(source, entry, paramTypes,
                        [&](const ASTNode &fn, const std::vector<ParamSpec> &params,
                            const TransferRegistry &reg, const FunctionTable &table,
                            const ClassRegistry *classes) {
                            return emitProgram(fn, params, table, reg, classes, bridge);
                        });
}

std::string transpileToPlugin(const std::string &source, const std::string &entry,
                              const std::vector<InferredType> &paramTypes,
                              const std::string &exportName, const std::string &abiHeaderPath)
{
    return withPrepared(source, entry, paramTypes,
                        [&](const ASTNode &fn, const std::vector<ParamSpec> &params,
                            const TransferRegistry &reg, const FunctionTable &,
                            const ClassRegistry *classes) {
                            return emitScalarPlugin(fn, params, reg, exportName, abiHeaderPath,
                                                    classes);
                        });
}

// Parse the return type out of an EmittedFunction signature of the form
// `<retty> <name>(<params>)`. The emitter uses fixed-width / single-token
// types (double, float, std::complex<double>, bool, int8_t..int64_t,
// uint8_t..uint64_t, void) — none contain spaces — so the return type is the
// substring before the last space that precedes the first `(`.
namespace {
std::string parseRetType(const std::string &signature)
{
    const auto paren = signature.find('(');
    if (paren == std::string::npos) return "";
    const std::string head = signature.substr(0, paren);  // "<retty> <name>"
    const auto sp = head.rfind(' ');
    if (sp == std::string::npos) return "";
    return head.substr(0, sp);
}
} // namespace

std::string buildHarnessMain(const EmittedFunction &em)
{
    const std::string &retty = parseRetType(em.signature);
    std::string body;
    if (retty == "void") {
        body = "    " + em.name + "();\n";
    } else if (retty == "double" || retty == "float" || retty == "bool" ||
               retty == "int8_t" || retty == "int16_t" || retty == "int32_t" ||
               retty == "int64_t" || retty == "uint8_t" || retty == "uint16_t" ||
               retty == "uint32_t" || retty == "uint64_t") {
        body = "    " + retty + " _r = " + em.name + "();\n"
               "    std::printf(\"%.17g\\n\", static_cast<double>(_r));\n";
    } else if (retty == "std::complex<double>") {
        body = "    std::complex<double> _r = " + em.name + "();\n"
               "    std::printf(\"%.17g%+.17gi\\n\", _r.real(), _r.imag());\n";
    } else {
        throw std::runtime_error(
            "numkit build: cannot build a run harness for return type '" + retty +
            "' (v1 supports only scalar / complex / void returns). Use `coder` / "
            "`-o` to get the C++ and write your own main().");
    }
    return "#include <cstdio>\n"
           "#include <complex>\n"
           "int main() {\n" +
           body +
           "    return 0;\n"
           "}\n";
}

} // namespace numkit::codegen::driver
