// codegen/src/emitter.cpp — see emitter.hpp.

#include <numkit/codegen/emitter.hpp>

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace numkit::codegen {

std::string cppScalarType(ValueType dtype)
{
    switch (dtype) {
    case ValueType::DOUBLE:  return "double";
    case ValueType::SINGLE:  return "float";
    case ValueType::COMPLEX: return "std::complex<double>";
    case ValueType::LOGICAL: return "bool";
    case ValueType::INT8:    return "std::int8_t";
    case ValueType::INT16:   return "std::int16_t";
    case ValueType::INT32:   return "std::int32_t";
    case ValueType::INT64:   return "std::int64_t";
    case ValueType::UINT8:   return "std::uint8_t";
    case ValueType::UINT16:  return "std::uint16_t";
    case ValueType::UINT32:  return "std::uint32_t";
    case ValueType::UINT64:  return "std::uint64_t";
    default:
        throw std::runtime_error("cppScalarType: no scalar C++ mapping for this dtype");
    }
}

std::string formatDoubleLiteral(double v)
{
    if (std::isnan(v)) return "std::numeric_limits<double>::quiet_NaN()";
    if (std::isinf(v))
        return v < 0 ? "-std::numeric_limits<double>::infinity()"
                     : "std::numeric_limits<double>::infinity()";

    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    std::string s(buf, ptr);
    // Ensure it reads as a double in C++ (e.g. "2" -> "2.0", not an int).
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos)
        s += ".0";
    return s;
}

namespace {

// MATLAB binary operator -> C++ operator for the SCALAR case. Returns
// empty for power (handled via std::pow) or an unhandled operator.
const char *cppScalarBinOp(const std::string &op)
{
    if (op == "+")  return "+";
    if (op == "-")  return "-";
    if (op == "*" || op == ".*") return "*";
    if (op == "/" || op == "./") return "/";
    if (op == "==") return "==";
    if (op == "~=" || op == "!=") return "!=";
    if (op == "<")  return "<";
    if (op == ">")  return ">";
    if (op == "<=") return "<=";
    if (op == ">=") return ">=";
    if (op == "&&" || op == "&") return "&&";  // scalar logical
    if (op == "||" || op == "|") return "||";
    return nullptr;  // ^, .^ -> std::pow; anything else -> unsupported
}

[[noreturn]] void unsupported(const char *what)
{
    throw std::runtime_error(std::string("emitScalarExpr: unsupported in this brick — ") + what);
}

} // namespace

std::string emitScalarExpr(const ASTNode &expr)
{
    switch (expr.type) {
    case NodeType::NUMBER_LITERAL:
        return formatDoubleLiteral(expr.numValue);

    case NodeType::IMAG_LITERAL:
        return "std::complex<double>(0.0, " + formatDoubleLiteral(expr.numValue) + ")";

    case NodeType::BOOL_LITERAL:
        return expr.boolValue ? "true" : "false";

    case NodeType::IDENTIFIER:
        return expr.strValue;  // a local C++ variable of the matching scalar type

    case NodeType::BINARY_OP: {
        if (expr.children.size() != 2) unsupported("binary op arity");
        const std::string l = emitScalarExpr(*expr.children[0]);
        const std::string r = emitScalarExpr(*expr.children[1]);
        if (expr.strValue == "^" || expr.strValue == ".^")
            return "std::pow(" + l + ", " + r + ")";
        const char *op = cppScalarBinOp(expr.strValue);
        if (!op) unsupported(("operator " + expr.strValue).c_str());
        return "(" + l + " " + op + " " + r + ")";
    }

    case NodeType::UNARY_OP: {
        if (expr.children.size() != 1) unsupported("unary op arity");
        const std::string a = emitScalarExpr(*expr.children[0]);
        if (expr.strValue == "-") return "(-" + a + ")";
        if (expr.strValue == "+") return "(+" + a + ")";
        if (expr.strValue == "~" || expr.strValue == "!") return "(!" + a + ")";
        unsupported(("unary operator " + expr.strValue).c_str());
    }

    default:
        unsupported("node kind (calls / indexing / matrices come later)");
    }
}

} // namespace numkit::codegen
