// src/MLabTreeWalker.cpp
#include "MLabTreeWalker.hpp"
#include "MLabEngine.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace mlab {

TreeWalker::TreeWalker(Engine &engine)
    : engine_(engine)
{}

MValue TreeWalker::execute(const ASTNode *ast, Environment *env)
{
    std::optional<DebugController::FrameGuard> dbgFrame;
    if (auto *ctl = debugCtl()) {
        ctl->reset();
        StackFrame frame;
        frame.functionName = topLevelName_;
        frame.env = env;
        dbgFrame.emplace(*ctl, std::move(frame));
    }

    return execNode(ast, env);
}

bool TreeWalker::isKnownFunction(const std::string &name) const
{
    static const std::unordered_set<std::string> kBuiltinFuncs = {"tic", "toc"};
    return engine_.externalFuncs_.count(name) || engine_.userFuncs_.count(name)
           || kBuiltinFuncs.count(name);
}

// ============================================================
void TreeWalker::output(const std::string &s)
{
    if (engine_.outputFunc_)
        engine_.outputFunc_(s);
    else
        std::cout << s;
}

void TreeWalker::displayValue(const std::string &name, const MValue &val)
{
    std::ostringstream os;
    if (!name.empty() && name != "ans")
        os << name << " =\n";

    switch (val.type()) {
    case MType::DOUBLE: {
        if (val.isScalar()) {
            os << "    " << val.toScalar() << "\n";
        } else {
            auto d = val.dims();
            for (size_t p = 0; p < d.pages(); ++p) {
                if (d.is3D())
                    os << "(:,:," << p + 1 << ") =\n";
                for (size_t r = 0; r < d.rows(); ++r) {
                    os << "   ";
                    for (size_t c = 0; c < d.cols(); ++c)
                        os << " " << val(r, c, p);
                    os << "\n";
                }
            }
        }
        break;
    }
    case MType::CHAR:
        os << "    '" << val.toString() << "'\n";
        break;
    case MType::LOGICAL: {
        if (val.isScalar()) {
            os << "    " << (val.toBool() ? "true" : "false") << "\n";
        } else {
            auto d = val.dims();
            const uint8_t *ld = val.logicalData();
            for (size_t r = 0; r < d.rows(); ++r) {
                os << "   ";
                for (size_t c = 0; c < d.cols(); ++c)
                    os << " " << (ld[d.sub2ind(r, c)] ? "1" : "0");
                os << "\n";
            }
        }
        break;
    }
    case MType::COMPLEX: {
        if (val.isScalar()) {
            auto c = val.toComplex();
            os << "   ";
            if (c.real() != 0.0 || c.imag() == 0.0)
                os << " " << c.real();
            if (c.imag() != 0.0) {
                if (c.real() != 0.0 && c.imag() > 0)
                    os << " +";
                else if (c.real() != 0.0)
                    os << " ";
                os << " " << c.imag() << "i";
            }
            os << "\n";
        } else {
            auto d = val.dims();
            for (size_t r = 0; r < d.rows(); ++r) {
                os << "   ";
                for (size_t cc = 0; cc < d.cols(); ++cc) {
                    auto v = val.complexElem(r, cc);
                    os << " " << v.real();
                    if (v.imag() >= 0)
                        os << "+";
                    os << v.imag() << "i";
                }
                os << "\n";
            }
        }
        break;
    }
    case MType::STRUCT:
        os << "  struct with fields:\n";
        for (auto &[k, v] : val.structFields())
            os << "    " << k << ": " << v.debugString() << "\n";
        break;
    case MType::FUNC_HANDLE:
        os << "    @" << val.funcHandleName() << "\n";
        break;
    case MType::CELL: {
        auto d = val.dims();
        os << "  {" << d.rows() << "x" << d.cols() << " cell}\n";
        for (size_t i = 0; i < val.numel() && i < 20; ++i)
            os << "    {" << i + 1 << "}: " << val.cellAt(i).debugString() << "\n";
        if (val.numel() > 20)
            os << "    ... (" << val.numel() - 20 << " more)\n";
        break;
    }
    case MType::STRING: {
        if (val.isScalar()) {
            os << "    \"" << val.toString() << "\"\n";
        } else {
            auto d = val.dims();
            os << "  " << d.rows() << "x" << d.cols() << " string array\n";
            for (size_t i = 0; i < val.numel() && i < 20; ++i)
                os << "    \"" << val.stringElem(i) << "\"\n";
        }
        break;
    }
    case MType::EMPTY:
        os << "    []\n";
        break;
    default:
        os << "    " << val.debugString() << "\n";
        break;
    }
    output(os.str());
}

// Returns true and sets `outIdx` if the index expression evaluates to a scalar integer.
bool TreeWalker::tryResolveScalarIndex(const ASTNode *indexExpr,
                                       const MValue &array,
                                       int dim,
                                       int ndims,
                                       Environment *env,
                                       size_t &outIdx)
{
    // Fast path for number literals: B(3)
    if (indexExpr->type == NodeType::NUMBER_LITERAL) {
        double v = indexExpr->numValue;
        if (v >= 1.0 && v == std::floor(v)) {
            outIdx = static_cast<size_t>(v) - 1;
            return true;
        }
        return false;
    }

    // Fast path for simple identifiers: B(i)
    if (indexExpr->type == NodeType::IDENTIFIER) {
        const std::string &name = indexExpr->strValue;
        if (name == "end") {
            outIdx = ((ndims == 1) ? array.numel() : array.dims().dimSize(dim)) - 1;
            return true;
        }
        auto *val = env->get(name);
        if (val && val->isScalar() && val->type() == MType::DOUBLE) {
            double v = val->toScalar();
            if (v >= 1.0 && v == std::floor(v)) {
                outIdx = static_cast<size_t>(v) - 1;
                return true;
            }
        }
        return false;
    }

    // Fast path for simple binary expressions on scalars: B(i+1), B(2*j)
    if (indexExpr->type == NodeType::BINARY_OP && indexExpr->cachedOp) {
        auto *left = indexExpr->children[0].get();
        auto *right = indexExpr->children[1].get();

        // Only handle two-identifier or identifier-literal combos
        double lv, rv;
        bool lOk = false, rOk = false;

        if (left->type == NodeType::NUMBER_LITERAL) {
            lv = left->numValue;
            lOk = true;
        } else if (left->type == NodeType::IDENTIFIER) {
            auto *val = env->get(left->strValue);
            if (val && val->isScalar() && val->type() == MType::DOUBLE) {
                lv = val->toScalar();
                lOk = true;
            }
        }
        if (right->type == NodeType::NUMBER_LITERAL) {
            rv = right->numValue;
            rOk = true;
        } else if (right->type == NodeType::IDENTIFIER) {
            auto *val = env->get(right->strValue);
            if (val && val->isScalar() && val->type() == MType::DOUBLE) {
                rv = val->toScalar();
                rOk = true;
            }
        }

        if (lOk && rOk) {
            MValue lm = MValue::scalar(lv, &engine_.allocator_);
            MValue rm = MValue::scalar(rv, &engine_.allocator_);
            MValue result = (*static_cast<const BinaryOpFunc *>(indexExpr->cachedOp))(lm, rm);
            if (result.isScalar()) {
                double v = result.toScalar();
                if (v >= 1.0 && v == std::floor(v)) {
                    outIdx = static_cast<size_t>(v) - 1;
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<size_t> TreeWalker::resolveIndex(
    const ASTNode *indexExpr, const MValue &array, int dim, int ndims, Environment *env)
{
    IndexContextGuard guard(indexContextStack_, {&array, dim, ndims});

    MValue val = execNode(indexExpr, env);

    std::vector<size_t> indices;

    if (val.isChar() && val.toString() == ":") {
        size_t sz = (ndims == 1) ? array.numel() : array.dims().dimSize(dim);
        indices.resize(sz);
        for (size_t i = 0; i < sz; ++i)
            indices[i] = i;
        return indices;
    }

    if (val.isLogical()) {
        const uint8_t *ld = val.logicalData();
        for (size_t i = 0; i < val.numel(); ++i)
            if (ld[i])
                indices.push_back(i);
        return indices;
    }

    if (val.type() == MType::DOUBLE) {
        const double *dd = val.doubleData();
        indices.reserve(val.numel());
        for (size_t i = 0; i < val.numel(); ++i) {
            double idx = dd[i];
            if (idx < 1.0 || idx != std::floor(idx))
                throw std::runtime_error("Array indices must be positive integers, got "
                                         + std::to_string(idx));
            indices.push_back(static_cast<size_t>(idx) - 1);
        }
        return indices;
    }

    if (val.isNumeric()) {
        if (val.isScalar()) {
            double idx = val.toScalar();
            if (idx < 1.0)
                throw std::runtime_error("Array index must be positive integer");
            indices.push_back(static_cast<size_t>(idx) - 1);
            return indices;
        }
        throw std::runtime_error("Indexing with non-double numeric arrays not yet supported");
    }

    throw std::runtime_error("Invalid index type: " + std::string(mtypeName(val.type())));
}

// ============================================================
MValue TreeWalker::execNodeInner(const ASTNode *node, Environment *env)
{
    switch (node->type) {
    case NodeType::BLOCK:
        return execBlock(node, env);
    case NodeType::NUMBER_LITERAL:
        return MValue::scalar(node->numValue, &engine_.allocator_);
    case NodeType::STRING_LITERAL:
        return MValue::fromString(node->strValue, &engine_.allocator_);
    case NodeType::DQSTRING_LITERAL:
        return MValue::stringScalar(node->strValue, &engine_.allocator_);
    case NodeType::BOOL_LITERAL:
        return MValue::logicalScalar(node->boolValue, &engine_.allocator_);
    case NodeType::IMAG_LITERAL:
        return MValue::complexScalar(0.0, node->numValue, &engine_.allocator_);
    case NodeType::IDENTIFIER:
        return execIdentifier(node, env);
    case NodeType::ASSIGN:
        return execAssign(node, env);
    case NodeType::MULTI_ASSIGN:
        return execMultiAssign(node, env);
    case NodeType::BINARY_OP:
        return execBinaryOp(node, env);
    case NodeType::UNARY_OP:
        return execUnaryOp(node, env);
    case NodeType::CALL:
        return execCall(node, env);
    case NodeType::CELL_INDEX:
        return execCellIndex(node, env);
    case NodeType::FIELD_ACCESS:
        return execFieldAccess(node, env);
    case NodeType::DYNAMIC_FIELD_ACCESS: {
        auto obj = execNode(node->children[0].get(), env);
        if (!obj.isStruct())
            throw std::runtime_error("Dot indexing requires a struct");
        std::string fname = execNode(node->children[1].get(), env).toString();
        if (!obj.hasField(fname))
            throw std::runtime_error("Reference to non-existent field '" + fname + "'");
        return obj.field(fname);
    }
    case NodeType::MATRIX_LITERAL:
        return execMatrixLiteral(node, env);
    case NodeType::CELL_LITERAL:
        return execCellLiteral(node, env);
    case NodeType::COLON_EXPR:
        return execColonExpr(node, env);
    case NodeType::IF_STMT:
        return execIf(node, env);
    case NodeType::FOR_STMT:
        return execFor(node, env);
    case NodeType::WHILE_STMT:
        return execWhile(node, env);
    case NodeType::SWITCH_STMT:
        return execSwitch(node, env);
    case NodeType::BREAK_STMT:
        flowSignal_ = FlowSignal::BREAK;
        return MValue::empty();
    case NodeType::CONTINUE_STMT:
        flowSignal_ = FlowSignal::CONTINUE;
        return MValue::empty();
    case NodeType::RETURN_STMT:
        flowSignal_ = FlowSignal::RETURN;
        return MValue::empty();
    case NodeType::FUNCTION_DEF:
        return execFunctionDef(node, env);
    case NodeType::EXPR_STMT:
        return execExprStmt(node, env);
    case NodeType::ANON_FUNC:
        return execAnonFunc(node, env);
    case NodeType::TRY_STMT:
        return execTryCatch(node, env);
    case NodeType::DELETE_ASSIGN:
        return execDeleteAssign(node, env);
    case NodeType::GLOBAL_STMT:
    case NodeType::PERSISTENT_STMT:
        return execGlobalPersistent(node, env);
    case NodeType::COMMAND_CALL:
        return execCommandCall(node, env);
    case NodeType::END_VAL: {
        if (!indexContextStack_.empty()) {
            auto &ctx = indexContextStack_.back();
            size_t sz = (ctx.ndims == 1) ? ctx.array->numel()
                                         : ctx.array->dims().dimSize(ctx.dimension);
            return MValue::scalar(static_cast<double>(sz), &engine_.allocator_);
        }
        throw std::runtime_error("'end' used outside of indexing context");
    }
    default:
        throw std::runtime_error("Unknown AST node type");
    }
}

static std::string describeNode(const ASTNode *node)
{
    switch (node->type) {
    case NodeType::CALL:
        if (!node->strValue.empty())
            return "in call to '" + node->strValue + "'";
        if (!node->children.empty() && node->children[0]->type == NodeType::IDENTIFIER)
            return "in call to '" + node->children[0]->strValue + "'";
        return "in function call";
    case NodeType::CELL_INDEX:
        if (!node->children.empty() && node->children[0]->type == NodeType::IDENTIFIER)
            return "in cell indexing of '" + node->children[0]->strValue + "'";
        return "in cell indexing";
    case NodeType::BINARY_OP:
        return "in operator '" + node->strValue + "'";
    case NodeType::UNARY_OP:
        return "in unary operator '" + node->strValue + "'";
    case NodeType::FIELD_ACCESS:
        return "in field access '." + node->strValue + "'";
    case NodeType::DYNAMIC_FIELD_ACCESS:
        return "in dynamic field access";
    case NodeType::ASSIGN:
        if (!node->children.empty() && node->children[0]->type == NodeType::IDENTIFIER)
            return "in assignment to '" + node->children[0]->strValue + "'";
        return "in assignment";
    case NodeType::COLON_EXPR:
        return "in colon expression";
    case NodeType::MATRIX_LITERAL:
        return "in matrix construction";
    case NodeType::CELL_LITERAL:
        return "in cell construction";
    case NodeType::IDENTIFIER:
        return "'" + node->strValue + "'";
    case NodeType::EXPR_STMT:
        if (!node->children.empty())
            return describeNode(node->children[0].get());
        return "";
    default:
        return "";
    }
}

MValue TreeWalker::execNode(const ASTNode *node, Environment *env)
{
    if (!node)
        return MValue::empty();

    try {
        return execNodeInner(node, env);
    } catch (const MLabError &) {
        throw;
    } catch (const DebugStopException &) {
        throw;
    } catch (const std::runtime_error &e) {
        if (node->line > 0)
            throw MLabError(e.what(), node->line, node->col, "", describeNode(node));
        throw;
    }
}

// Returns true and sets `out` on success, false on failure (caller falls back to execNode).
// ============================================================
// tryEvalFast — fast-path evaluation returning MValue
//
// Handles double scalars, logical scalars, and comparison ops
// without falling through to the full execNode path.
// Returns true if the expression was evaluated successfully.
// ============================================================

// Helper: extract a double scalar from an MValue (double or logical)
static inline bool asDouble(const MValue &v, double &d)
{
    if (v.isDoubleScalar()) {
        d = v.scalarVal();
        return true;
    }
    if (v.isLogicalScalar()) {
        d = v.fastScalarVal();
        return true;
    }
    return false;
}

bool TreeWalker::tryEvalFast(const ASTNode *expr, Environment *env, MValue &out)
{
    switch (expr->type) {
    case NodeType::NUMBER_LITERAL:
        out.setScalarFast(expr->numValue);
        return true;

    case NodeType::IDENTIFIER: {
        MValue *v = env->get(expr->strValue);
        if (v && v->isScalar()) {
            MType t = v->type();
            if (t == MType::DOUBLE) {
                out.setScalarFast(v->toScalar());
                return true;
            }
            if (t == MType::LOGICAL) {
                out.setLogicalFast(v->toBool());
                return true;
            }
        }
        return false;
    }

    case NodeType::FIELD_ACCESS: {
        // p.x where p is a struct and p.x is a scalar double/logical
        if (expr->children.size() == 1 && expr->children[0]->type == NodeType::IDENTIFIER) {
            MValue *obj = env->get(expr->children[0]->strValue);
            if (obj && obj->isStruct() && obj->hasField(expr->strValue)) {
                const MValue &fv = obj->field(expr->strValue);
                if (fv.isScalar()) {
                    MType t = fv.type();
                    if (t == MType::DOUBLE) {
                        out.setScalarFast(fv.toScalar());
                        return true;
                    }
                    if (t == MType::LOGICAL) {
                        out.setLogicalFast(fv.toBool());
                        return true;
                    }
                }
            }
        }
        return false;
    }

    case NodeType::BINARY_OP: {
        if (expr->children.size() != 2)
            return false;
        // Short-circuit operators must NOT use fast-path
        // (both operands would be evaluated before checking the operator)
        const std::string &opStr = expr->strValue;
        if (opStr == "&&" || opStr == "||")
            return false;

        MValue lm, rm;
        if (!tryEvalFast(expr->children[0].get(), env, lm))
            return false;
        if (!tryEvalFast(expr->children[1].get(), env, rm))
            return false;

        // Extract double values for arithmetic (works for both double and logical scalars)
        double lv, rv;
        bool lOk = asDouble(lm, lv);
        bool rOk = asDouble(rm, rv);
        if (!lOk || !rOk)
            return false;

        // Direct scalar arithmetic — bypass std::function overhead
        if (opStr.size() == 1) {
            switch (opStr[0]) {
            case '+':
                out.setScalarFast(lv + rv);
                return true;
            case '-':
                out.setScalarFast(lv - rv);
                return true;
            case '*':
                out.setScalarFast(lv * rv);
                return true;
            case '/':
                out.setScalarFast(lv / rv);
                return true;
            case '^':
                out.setScalarFast(std::pow(lv, rv));
                return true;
            case '<':
                out.setLogicalFast(lv < rv);
                return true;
            case '>':
                out.setLogicalFast(lv > rv);
                return true;
            default:
                break;
            }
        } else if (opStr == ".*") {
            out.setScalarFast(lv * rv);
            return true;
        } else if (opStr == "./") {
            out.setScalarFast(lv / rv);
            return true;
        } else if (opStr == ".^") {
            out.setScalarFast(std::pow(lv, rv));
            return true;
        } else if (opStr == "<=") {
            out.setLogicalFast(lv <= rv);
            return true;
        } else if (opStr == ">=") {
            out.setLogicalFast(lv >= rv);
            return true;
        } else if (opStr == "==") {
            out.setLogicalFast(lv == rv);
            return true;
        } else if (opStr == "~=") {
            out.setLogicalFast(lv != rv);
            return true;
        }

        // Unknown op — fall back to cached BinaryOpFunc
        if (!expr->cachedOp)
            return false;
        MValue lArg = MValue::scalar(lv, &engine_.allocator_);
        MValue rArg = MValue::scalar(rv, &engine_.allocator_);
        MValue result = (*static_cast<const BinaryOpFunc *>(expr->cachedOp))(lArg, rArg);
        if (result.isScalar()) {
            MType t = result.type();
            if (t == MType::DOUBLE || t == MType::LOGICAL) {
                out = std::move(result);
                return true;
            }
        }
        return false;
    }

    case NodeType::UNARY_OP: {
        if (expr->children.size() != 1)
            return false;
        MValue operandM;
        if (!tryEvalFast(expr->children[0].get(), env, operandM))
            return false;

        double operand;
        if (!asDouble(operandM, operand))
            return false;

        const std::string &op = expr->strValue;
        if (op == "-") {
            out.setScalarFast(-operand);
            return true;
        }
        if (op == "+") {
            out.setScalarFast(operand);
            return true;
        }
        if (op == "~") {
            out.setLogicalFast(operand == 0.0);
            return true;
        }

        if (!expr->cachedOp)
            return false;
        MValue om = MValue::scalar(operand, &engine_.allocator_);
        MValue result = (*static_cast<const UnaryOpFunc *>(expr->cachedOp))(om);
        if (result.isScalar()) {
            MType t = result.type();
            if (t == MType::DOUBLE || t == MType::LOGICAL) {
                out = std::move(result);
                return true;
            }
        }
        return false;
    }

    case NodeType::CALL: {
        // func(a, b, ...) where all args are scalar
        if (expr->children.empty())
            return false;
        auto *funcNode = expr->children[0].get();
        size_t nargs = expr->children.size() - 1;
        if (nargs > 4)
            return false;

        // Resolve builtin ID on first call (0 = unresolved)
        if (funcNode->cachedBuiltinId == 0 && funcNode->type == NodeType::IDENTIFIER) {
            const auto &fn = funcNode->strValue;
            // IDs: 1-19 = known builtins, -1 = not a scalar builtin
            if (fn == "mod")
                funcNode->cachedBuiltinId = 1;
            else if (fn == "abs")
                funcNode->cachedBuiltinId = 2;
            else if (fn == "floor")
                funcNode->cachedBuiltinId = 3;
            else if (fn == "ceil")
                funcNode->cachedBuiltinId = 4;
            else if (fn == "round")
                funcNode->cachedBuiltinId = 5;
            else if (fn == "fix")
                funcNode->cachedBuiltinId = 6;
            else if (fn == "sin")
                funcNode->cachedBuiltinId = 7;
            else if (fn == "cos")
                funcNode->cachedBuiltinId = 8;
            else if (fn == "sqrt")
                funcNode->cachedBuiltinId = 9;
            else if (fn == "exp")
                funcNode->cachedBuiltinId = 10;
            else if (fn == "log")
                funcNode->cachedBuiltinId = 11;
            else if (fn == "min")
                funcNode->cachedBuiltinId = 12;
            else if (fn == "max")
                funcNode->cachedBuiltinId = 13;
            else if (fn == "sign")
                funcNode->cachedBuiltinId = 14;
            else if (fn == "tan")
                funcNode->cachedBuiltinId = 15;
            else if (fn == "log2")
                funcNode->cachedBuiltinId = 16;
            else if (fn == "log10")
                funcNode->cachedBuiltinId = 17;
            else if (fn == "rem")
                funcNode->cachedBuiltinId = 18;
            else
                funcNode->cachedBuiltinId = -1;
        }

        int8_t bid = funcNode->cachedBuiltinId;
        if (bid > 0) {
            MValue argMs[4];
            double argVals[4];
            for (size_t i = 0; i < nargs; ++i) {
                if (!tryEvalFast(expr->children[i + 1].get(), env, argMs[i]))
                    return false;
                if (!asDouble(argMs[i], argVals[i]))
                    return false;
            }
            double r;
            bool ok = false;
            switch (bid) {
            case 1:
                if (nargs == 2) {
                    r = std::fmod(argVals[0], argVals[1]);
                    if (r != 0 && ((r < 0) != (argVals[1] < 0)))
                        r += argVals[1];
                    ok = true;
                }
                break;
            case 2:
                if (nargs == 1) {
                    r = std::abs(argVals[0]);
                    ok = true;
                }
                break;
            case 3:
                if (nargs == 1) {
                    r = std::floor(argVals[0]);
                    ok = true;
                }
                break;
            case 4:
                if (nargs == 1) {
                    r = std::ceil(argVals[0]);
                    ok = true;
                }
                break;
            case 5:
                if (nargs == 1) {
                    r = std::round(argVals[0]);
                    ok = true;
                }
                break;
            case 6:
                if (nargs == 1) {
                    r = std::trunc(argVals[0]);
                    ok = true;
                }
                break;
            case 7:
                if (nargs == 1) {
                    r = std::sin(argVals[0]);
                    ok = true;
                }
                break;
            case 8:
                if (nargs == 1) {
                    r = std::cos(argVals[0]);
                    ok = true;
                }
                break;
            case 9:
                if (nargs == 1 && argVals[0] >= 0) {
                    r = std::sqrt(argVals[0]);
                    ok = true;
                }
                break;
            case 10:
                if (nargs == 1) {
                    r = std::exp(argVals[0]);
                    ok = true;
                }
                break;
            case 11:
                if (nargs == 1) {
                    r = std::log(argVals[0]);
                    ok = true;
                }
                break;
            case 12:
                if (nargs == 2) {
                    r = std::fmin(argVals[0], argVals[1]);
                    ok = true;
                } else if (nargs == 1) {
                    r = argVals[0];
                    ok = true;
                }
                break;
            case 13:
                if (nargs == 2) {
                    r = std::fmax(argVals[0], argVals[1]);
                    ok = true;
                } else if (nargs == 1) {
                    r = argVals[0];
                    ok = true;
                }
                break;
            case 14:
                if (nargs == 1) {
                    r = std::isnan(argVals[0]) ? std::numeric_limits<double>::quiet_NaN()
                        : (argVals[0] > 0) ? 1.0
                        : (argVals[0] < 0) ? -1.0
                                           : 0.0;
                    ok = true;
                }
                break;
            case 15:
                if (nargs == 1) {
                    r = std::tan(argVals[0]);
                    ok = true;
                }
                break;
            case 16:
                if (nargs == 1) {
                    r = std::log2(argVals[0]);
                    ok = true;
                }
                break;
            case 17:
                if (nargs == 1) {
                    r = std::log10(argVals[0]);
                    ok = true;
                }
                break;
            case 18:
                if (nargs == 2) {
                    r = std::fmod(argVals[0], argVals[1]);
                    ok = true;
                }
                break;
            }
            if (ok) {
                out.setScalarFast(r);
                return true;
            }
            // nargs mismatch — fall through to ExternalFunc path
        }

        if (!funcNode->cachedOp) {
            // Try array scalar indexing: A(i) or A(i,j) where A is a double array
            if (funcNode->type == NodeType::IDENTIFIER) {
                if (nargs == 1) {
                    MValue idxM;
                    if (tryEvalFast(expr->children[1].get(), env, idxM)) {
                        double idxVal;
                        if (asDouble(idxM, idxVal)) {
                            MValue *arr = env->get(funcNode->strValue);
                            if (arr && arr->type() == MType::DOUBLE) {
                                size_t idx = static_cast<size_t>(idxVal) - 1;
                                if (idx < arr->numel()) {
                                    out.setScalarFast(arr->doubleData()[idx]);
                                    return true;
                                }
                            }
                        }
                    }
                } else if (nargs == 2) {
                    MValue rowM, colM;
                    if (tryEvalFast(expr->children[1].get(), env, rowM)
                        && tryEvalFast(expr->children[2].get(), env, colM)) {
                        double rowVal, colVal;
                        if (asDouble(rowM, rowVal) && asDouble(colM, colVal)) {
                            MValue *arr = env->get(funcNode->strValue);
                            if (arr && arr->type() == MType::DOUBLE) {
                                size_t r = static_cast<size_t>(rowVal) - 1;
                                size_t c = static_cast<size_t>(colVal) - 1;
                                if (r < arr->dims().rows() && c < arr->dims().cols()) {
                                    out.setScalarFast(arr->doubleData()[arr->dims().sub2ind(r, c)]);
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            return false;
        }
        // ExternalFunc fast path — all args must be scalar doubles
        MValue argMs[4];
        double argVals[4];
        for (size_t i = 0; i < nargs; ++i) {
            if (!tryEvalFast(expr->children[i + 1].get(), env, argMs[i]))
                return false;
            if (!asDouble(argMs[i], argVals[i]))
                return false;
        }
        // Reuse engine-owned buffer to avoid heap allocation per call
        callArgsBuf_.clear();
        for (size_t i = 0; i < nargs; ++i)
            callArgsBuf_.push_back(MValue::scalar(argVals[i], &engine_.allocator_));
        MValue outBuf[1];
        CallContext ctx{&engine_, env};
        (*static_cast<const ExternalFunc *>(
            funcNode->cachedOp))(callArgsBuf_, 1, Span<MValue>(outBuf, 1), ctx);
        if (outBuf[0].isScalar()) {
            MType t = outBuf[0].type();
            if (t == MType::DOUBLE || t == MType::LOGICAL) {
                out = std::move(outBuf[0]);
                return true;
            }
        }
        return false;
    }

    default:
        return false;
    }
}

MValue TreeWalker::execBlock(const ASTNode *node, Environment *env)
{
    MValue last = MValue::empty();
    for (auto &child : node->children) {
        // ── Debug hook: check for line change, breakpoints ──
        if (auto *ctl = debugCtl()) {
            if (child->line > 0) {
                if (!ctl->checkLine(static_cast<uint16_t>(child->line),
                                    static_cast<uint16_t>(child->col),
                                    currentRecursionDepth_))
                    throw DebugStopException();
            }
        }

        // ── Fast paths for ASSIGN with suppressed output ──
        if (child->type == NodeType::ASSIGN && child->suppressOutput
            && child->children.size() == 2) {
            const auto *lhsNode = child->children[0].get();
            const auto *rhsNode = child->children[1].get();

            // x = <scalar expr>
            if (lhsNode->type == NodeType::IDENTIFIER) {
                const std::string &lhsName = lhsNode->strValue;
                MValue fastVal;
                if (tryEvalFast(rhsNode, env, fastVal)) {
                    if (fastVal.isDoubleScalar()) {
                        // Double scalar — try in-place update
                        double dv = fastVal.scalarVal();
                        MValue *existing = env->getLocal(lhsName);
                        if (existing && existing->isDoubleScalar()) {
                            existing->setScalarVal(dv);
                            last = *existing;
                        } else {
                            env->set(lhsName, MValue::scalar(dv, &engine_.allocator_));
                            last = *env->get(lhsName);
                        }
                    } else {
                        // Logical scalar — preserve type
                        env->set(lhsName, std::move(fastVal));
                        last = *env->get(lhsName);
                    }
                    continue;
                }
                // x = A(i) — scalar indexed read
                if (rhsNode->type == NodeType::CALL && rhsNode->children.size() == 2
                    && rhsNode->children[0]->type == NodeType::IDENTIFIER) {
                    MValue *arr = env->get(rhsNode->children[0]->strValue);
                    if (arr && arr->type() == MType::DOUBLE) {
                        MValue idxM;
                        if (tryEvalFast(rhsNode->children[1].get(), env, idxM)) {
                            double idxVal;
                            if (asDouble(idxM, idxVal)) {
                                size_t idx = static_cast<size_t>(idxVal) - 1;
                                if (idx < arr->numel()) {
                                    double v = arr->doubleData()[idx];
                                    MValue *existing = env->getLocal(lhsName);
                                    if (existing && existing->isDoubleScalar()) {
                                        existing->setScalarVal(v);
                                    } else {
                                        env->set(lhsName, MValue::scalar(v, &engine_.allocator_));
                                    }
                                    continue;
                                }
                            }
                        }
                    }
                }
            }

            // A(i) = <scalar> or A(i,j) = <scalar>
            if (lhsNode->type == NodeType::CALL
                && lhsNode->children[0]->type == NodeType::IDENTIFIER) {
                size_t lhsArgs = lhsNode->children.size();
                const std::string &arrName = lhsNode->children[0]->strValue;
                MValue *arr = env->get(arrName);

                if (arr && arr->type() == MType::DOUBLE) {
                    if (lhsArgs == 2) {
                        // 1D: A(i) = val
                        MValue idxM, rhsM;
                        if (tryEvalFast(lhsNode->children[1].get(), env, idxM)
                            && tryEvalFast(rhsNode, env, rhsM)) {
                            double idxVal, rhsVal;
                            if (asDouble(idxM, idxVal) && asDouble(rhsM, rhsVal)
                                && idxVal >= 1.0 && idxVal == std::floor(idxVal)
                                && !std::isinf(idxVal)) {
                                size_t idx = static_cast<size_t>(idxVal) - 1;
                                if (idx < arr->numel()) {
                                    arr->doubleDataMut()[idx] = rhsVal;
                                } else {
                                    arr->ensureSize(idx, &engine_.allocator_);
                                    arr->doubleDataMut()[idx] = rhsVal;
                                }
                                continue;
                            }
                        }
                    } else if (lhsArgs == 3) {
                        // 2D: A(i,j) = val
                        MValue rowM, colM, rhsM;
                        if (tryEvalFast(lhsNode->children[1].get(), env, rowM)
                            && tryEvalFast(lhsNode->children[2].get(), env, colM)
                            && tryEvalFast(rhsNode, env, rhsM)) {
                            double rowVal, colVal, rhsVal;
                            if (asDouble(rowM, rowVal) && asDouble(colM, colVal)
                                && asDouble(rhsM, rhsVal)) {
                                size_t r = static_cast<size_t>(rowVal) - 1;
                                size_t c = static_cast<size_t>(colVal) - 1;
                                if (r < arr->dims().rows() && c < arr->dims().cols()) {
                                    arr->doubleDataMut()[arr->dims().sub2ind(r, c)] = rhsVal;
                                    continue;
                                }
                            }
                        }
                    }
                }
            }

            // p.x = <scalar> — struct field scalar assign
            if (lhsNode->type == NodeType::FIELD_ACCESS && lhsNode->children.size() == 1
                && lhsNode->children[0]->type == NodeType::IDENTIFIER) {
                MValue fastVal;
                if (tryEvalFast(rhsNode, env, fastVal)) {
                    if (fastVal.isDoubleScalar()) {
                        double dv = fastVal.scalarVal();
                        MValue *obj = env->get(lhsNode->children[0]->strValue);
                        if (obj && obj->isStruct()) {
                            MValue &fv = obj->field(lhsNode->strValue);
                            if (fv.isScalar() && fv.type() == MType::DOUBLE) {
                                *fv.doubleDataMut() = dv;
                                continue;
                            }
                            fv = MValue::scalar(dv, &engine_.allocator_);
                            continue;
                        }
                    } else {
                        // Logical or other fast type — set directly
                        MValue *obj = env->get(lhsNode->children[0]->strValue);
                        if (obj && obj->isStruct()) {
                            obj->field(lhsNode->strValue) = std::move(fastVal);
                            continue;
                        }
                    }
                }
            }
        }

        last = execNode(child.get(), env);
        if (flowSignal_ != FlowSignal::NONE)
            return last;
    }
    return last;
}

// ============================================================
MValue TreeWalker::execIdentifier(const ASTNode *node, Environment *env, size_t nargout)
{
    const std::string &name = node->strValue;

    auto *val = env->get(name);
    if (val)
        return *val;

    if (engine_.externalFuncs_.count(name)) {
        MValue outBuf[1];
        CallContext ctx{&engine_, env};
        engine_.externalFuncs_[name]({}, nargout, Span<MValue>(outBuf, 1), ctx);
        return outBuf[0].isEmpty() ? MValue::empty() : outBuf[0];
    }
    {
        auto _uit = engine_.userFuncs_.find(name);
        if (_uit != engine_.userFuncs_.end())
            return callUserFunction(_uit->second, {}, env);
    }

    throw std::runtime_error("Undefined variable or function: " + name);
}

// ============================================================
MValue TreeWalker::execAssign(const ASTNode *node, Environment *env)
{
    auto *lhs = node->children[0].get();
    auto rhs = execNode(node->children[1].get(), env);

    if (lhs->type == NodeType::IDENTIFIER) {
        // Fast path: if variable already exists as a scalar double
        // and rhs is scalar double, write in-place (no hash lookup for set)
        if (rhs.isScalar() && rhs.type() == MType::DOUBLE) {
            MValue *existing = env->getLocal(lhs->strValue);
            if (existing && existing->isScalar() && existing->type() == MType::DOUBLE) {
                *existing->doubleDataMut() = rhs.toScalar();
                if (!node->suppressOutput)
                    displayValue(lhs->strValue, *existing);
                return *existing;
            }
        }
        env->set(lhs->strValue, rhs);
        if (!node->suppressOutput)
            displayValue(lhs->strValue, rhs);
    } else if (lhs->type == NodeType::CALL) {
        execIndexedAssign(lhs, rhs, env);
    } else if (lhs->type == NodeType::FIELD_ACCESS) {
        execFieldAssign(lhs, rhs, env);
    } else if (lhs->type == NodeType::DYNAMIC_FIELD_ACCESS) {
        // s.(expr) = val
        auto *objNode = lhs->children[0].get();
        std::string fname = execNode(lhs->children[1].get(), env).toString();
        if (objNode->type == NodeType::IDENTIFIER) {
            auto *var = env->get(objNode->strValue);
            if (!var) {
                env->set(objNode->strValue, MValue::structure());
                var = env->get(objNode->strValue);
            }
            if (!var->isStruct())
                *var = MValue::structure();
            var->field(fname) = rhs;
        } else {
            throw std::runtime_error("Dynamic field assign: unsupported target");
        }
    } else if (lhs->type == NodeType::CELL_INDEX) {
        execCellAssign(lhs, rhs, env);
    } else {
        throw std::runtime_error("Invalid assignment target");
    }
    return rhs;
}

void TreeWalker::execIndexedAssign(const ASTNode *lhs, const MValue &rhs, Environment *env)
{
    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Invalid indexed assignment target");

    const std::string &varName = target->strValue;
    auto *var = env->get(varName);
    if (!var) {
        env->set(varName, MValue::empty());
        var = env->get(varName);
    }

    size_t nargs = lhs->children.size() - 1;

    if (var->isChar() && rhs.isChar()) {
        if (nargs == 1) {
            auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
            // Grow char array if any index exceeds current size (space-fill)
            size_t maxIdx = 0;
            for (auto idx : indices)
                if (idx >= maxIdx) maxIdx = idx + 1;
            if (maxIdx > var->numel()) {
                bool isColVec = (var->dims().cols() == 1 && var->dims().rows() > 1);
                if (isColVec)
                    var->resize(maxIdx, 1, &engine_.allocator_);
                else
                    var->resize(1, maxIdx, &engine_.allocator_);
            }
            const std::string rs = rhs.toString();
            char *data = var->charDataMut();
            if (rs.size() == 1) {
                for (auto idx : indices)
                    data[idx] = rs[0];
            } else {
                for (size_t i = 0; i < indices.size() && i < rs.size(); ++i)
                    data[indices[i]] = rs[i];
            }
            return;
        }
    }

    if (nargs == 1) {
        // ── scalar fast path: B(i) = scalar ──
        if (rhs.isScalar()) {
            size_t scalarIdx;
            if (tryResolveScalarIndex(lhs->children[1].get(), *var, 0, 1, env, scalarIdx)) {
                var->ensureSize(scalarIdx, &engine_.allocator_);
                var->elemSet(scalarIdx, rhs);
                return;
            }
        }

        auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
        for (auto idx : indices)
            var->ensureSize(idx, &engine_.allocator_);
        var->indexSet(indices.data(), indices.size(), rhs);
    } else if (nargs == 2) {
        // ── scalar fast path: M(i,j) = scalar ──
        if (rhs.isScalar()) {
            size_t ri, ci;
            if (tryResolveScalarIndex(lhs->children[1].get(), *var, 0, 2, env, ri)
                && tryResolveScalarIndex(lhs->children[2].get(), *var, 1, 2, env, ci)) {
                size_t needR = ri + 1, needC = ci + 1;
                if (needR > var->dims().rows() || needC > var->dims().cols())
                    var->resize(std::max(var->dims().rows(), needR),
                                std::max(var->dims().cols(), needC),
                                &engine_.allocator_);
                var->elemSet(var->dims().sub2ind(ri, ci), rhs);
                return;
            }
        }

        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 2, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 2, env);

        size_t maxR = 0, maxC = 0;
        for (auto r : rowIdx)
            maxR = std::max(maxR, r + 1);
        for (auto c : colIdx)
            maxC = std::max(maxC, c + 1);
        if (maxR > var->dims().rows() || maxC > var->dims().cols())
            var->resize(std::max(var->dims().rows(), maxR),
                        std::max(var->dims().cols(), maxC),
                        &engine_.allocator_);

        var->indexSet2D(rowIdx.data(), rowIdx.size(),
                        colIdx.data(), colIdx.size(), rhs);
    } else if (nargs == 3) {
        // ── scalar fast path: A(r,c,p) = scalar ──
        if (rhs.isScalar()) {
            size_t ri, ci, pi;
            if (tryResolveScalarIndex(lhs->children[1].get(), *var, 0, 3, env, ri)
                && tryResolveScalarIndex(lhs->children[2].get(), *var, 1, 3, env, ci)
                && tryResolveScalarIndex(lhs->children[3].get(), *var, 2, 3, env, pi)) {
                size_t needR = ri + 1, needC = ci + 1, needP = pi + 1;
                if (needR > var->dims().rows() || needC > var->dims().cols()
                    || needP > var->dims().pages())
                    var->resize3d(std::max(var->dims().rows(), needR),
                                  std::max(var->dims().cols(), needC),
                                  std::max(var->dims().pages(), needP),
                                  &engine_.allocator_);
                var->elemSet(var->dims().sub2ind(ri, ci, pi), rhs);
                return;
            }
        }

        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 3, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 3, env);
        auto pageIdx = resolveIndex(lhs->children[3].get(), *var, 2, 3, env);

        size_t maxR = 0, maxC = 0, maxP = 0;
        for (auto r : rowIdx)
            maxR = std::max(maxR, r + 1);
        for (auto c : colIdx)
            maxC = std::max(maxC, c + 1);
        for (auto p : pageIdx)
            maxP = std::max(maxP, p + 1);
        if (maxR > var->dims().rows() || maxC > var->dims().cols() || maxP > var->dims().pages())
            var->resize3d(std::max(var->dims().rows(), maxR),
                          std::max(var->dims().cols(), maxC),
                          std::max(var->dims().pages(), maxP),
                          &engine_.allocator_);

        var->indexSet3D(rowIdx.data(), rowIdx.size(),
                        colIdx.data(), colIdx.size(),
                        pageIdx.data(), pageIdx.size(), rhs);
    }
}

MValue &TreeWalker::resolveFieldLValue(const ASTNode *node, Environment *env)
{
    auto *objNode = node->children[0].get();
    const std::string &fieldName = node->strValue;

    if (objNode->type == NodeType::IDENTIFIER) {
        auto *var = env->get(objNode->strValue);
        if (!var) {
            env->set(objNode->strValue, MValue::structure());
            var = env->get(objNode->strValue);
        }
        if (!var->isStruct())
            *var = MValue::structure();
        return var->field(fieldName);
    }
    if (objNode->type == NodeType::FIELD_ACCESS) {
        MValue &parent = resolveFieldLValue(objNode, env);
        if (!parent.isStruct())
            parent = MValue::structure();
        return parent.field(fieldName);
    }
    throw std::runtime_error("Invalid field assignment target");
}

void TreeWalker::execFieldAssign(const ASTNode *lhs, const MValue &rhs, Environment *env)
{
    resolveFieldLValue(lhs, env) = rhs;
}

void TreeWalker::execCellAssign(const ASTNode *lhs, const MValue &rhs, Environment *env)
{
    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Invalid cell assignment target");

    auto *var = env->get(target->strValue);
    if (!var) {
        env->set(target->strValue, MValue::cell(0, 0));
        var = env->get(target->strValue);
    }
    if (!var->isCell())
        throw std::runtime_error("Cell indexing on non-cell variable: " + target->strValue);

    size_t nidx = lhs->children.size() - 1;

    if (nidx == 1) {
        IndexContextGuard guard(indexContextStack_, {var, 0, 1});
        MValue idx = execNode(lhs->children[1].get(), env);
        size_t i = static_cast<size_t>(idx.toScalar()) - 1;

        if (i >= var->numel()) {
            size_t newSize = i + 1;
            auto newCell = MValue::cell(1, newSize);
            for (size_t k = 0; k < var->numel(); ++k)
                newCell.cellAt(k) = var->cellAt(k);
            *var = newCell;
        }
        var->cellAt(i) = rhs;
    } else if (nidx == 2) {
        MValue ridx, cidx;
        {
            IndexContextGuard guard(indexContextStack_, {var, 0, 2});
            ridx = execNode(lhs->children[1].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {var, 1, 2});
            cidx = execNode(lhs->children[2].get(), env);
        }
        size_t r = static_cast<size_t>(ridx.toScalar()) - 1;
        size_t c = static_cast<size_t>(cidx.toScalar()) - 1;
        size_t idx = var->dims().sub2indChecked(r, c);
        var->cellAt(idx) = rhs;
    } else if (nidx == 3) {
        MValue ridx, cidx, pidx;
        {
            IndexContextGuard guard(indexContextStack_, {var, 0, 3});
            ridx = execNode(lhs->children[1].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {var, 1, 3});
            cidx = execNode(lhs->children[2].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {var, 2, 3});
            pidx = execNode(lhs->children[3].get(), env);
        }
        size_t r = static_cast<size_t>(ridx.toScalar()) - 1;
        size_t c = static_cast<size_t>(cidx.toScalar()) - 1;
        size_t p = static_cast<size_t>(pidx.toScalar()) - 1;
        size_t idx = var->dims().sub2indChecked(r, c, p);
        var->cellAt(idx) = rhs;
    } else {
        throw std::runtime_error("Cell assignment with more than 3 indices not supported");
    }
}

// ============================================================
MValue TreeWalker::execMultiAssign(const ASTNode *node, Environment *env)
{
    auto results = execCallMulti(node->children[0].get(), env, node->returnNames.size());

    for (size_t i = 0; i < node->returnNames.size() && i < results.size(); ++i)
        if (node->returnNames[i] != "~")
            env->set(node->returnNames[i], results[i]);

    if (!node->suppressOutput && !results.empty())
        for (size_t i = 0; i < node->returnNames.size() && i < results.size(); ++i)
            if (node->returnNames[i] != "~")
                displayValue(node->returnNames[i], results[i]);

    return results.empty() ? MValue::empty() : results[0];
}

std::vector<MValue> TreeWalker::execCallMulti(const ASTNode *node, Environment *env, size_t nout)
{
    // Cell CSL: [a,b] = c{idx} — expand cell elements into separate outputs
    if (node->type == NodeType::CELL_INDEX) {
        const MValue *cell = env->get(node->children[0]->strValue);
        if (!cell || !cell->isCell())
            throw std::runtime_error("Cell indexing requires a cell array");

        size_t nidx = node->children.size() - 1;
        if (nidx == 1) {
            auto indices = resolveIndex(node->children[1].get(), *cell, 0, 1, env);
            std::vector<MValue> out;
            out.reserve(indices.size());
            for (size_t idx : indices)
                out.push_back(cell->cellAt(idx));
            return out;
        } else if (nidx == 2) {
            auto rowIdx = resolveIndex(node->children[1].get(), *cell, 0, 2, env);
            auto colIdx = resolveIndex(node->children[2].get(), *cell, 1, 2, env);
            std::vector<MValue> out;
            for (size_t c : colIdx)
                for (size_t r : rowIdx)
                    out.push_back(cell->cellAt(cell->dims().sub2ind(r, c)));
            return out;
        } else {
            throw std::runtime_error("Cell CSL with " + std::to_string(nidx) + " indices not supported");
        }
    }

    if (node->type != NodeType::CALL)
        throw std::runtime_error("Expected function call in multi-assignment");

    const std::string &funcName = node->children[0]->strValue;

    std::vector<MValue> args;
    args.reserve(node->children.size() - 1);
    for (size_t i = 1; i < node->children.size(); ++i)
        args.push_back(execNode(node->children[i].get(), env));

    auto *var = env->get(funcName);
    if (var && var->isFuncHandle())
        return callFuncHandleMulti(*var, args, env, nout);

    // Fast path: cached function pointer
    auto *funcNode = node->children[0].get();
    if (funcNode->cachedOp) {
        std::vector<MValue> outBuf(nout);
        CallContext ctx{&engine_, env};
        (*static_cast<const ExternalFunc *>(
            funcNode->cachedOp))(args, nout, Span<MValue>(outBuf), ctx);
        return outBuf;
    }

    auto it = engine_.externalFuncs_.find(funcName);
    if (it != engine_.externalFuncs_.end()) {
        funcNode->cachedOp = &it->second;
        std::vector<MValue> outBuf(nout);
        CallContext ctx{&engine_, env};
        it->second(args, nout, Span<MValue>(outBuf), ctx);
        return outBuf;
    }
    {
        auto _uit = engine_.userFuncs_.find(funcName);
        if (_uit != engine_.userFuncs_.end())
            return callUserFunctionMulti(_uit->second, args, env, nout);
    }

    throw std::runtime_error("Undefined function: " + funcName);
}

// ============================================================
MValue TreeWalker::execDeleteAssign(const ASTNode *node, Environment *env)
{
    auto *lhs = node->children[0].get();
    if (lhs->type != NodeType::CALL || lhs->children.empty())
        throw std::runtime_error("Invalid delete assignment syntax");

    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Invalid delete target");

    auto *var = env->get(target->strValue);
    if (!var)
        throw std::runtime_error("Undefined variable: " + target->strValue);

    size_t nargs = lhs->children.size() - 1;

    if (nargs == 1) {
        auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
        var->indexDelete(indices.data(), indices.size(), &engine_.allocator_);
    } else if (nargs == 2) {
        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 2, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 2, env);
        var->indexDelete2D(rowIdx.data(), rowIdx.size(),
                           colIdx.data(), colIdx.size(),
                           &engine_.allocator_);
    } else if (nargs == 3) {
        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 3, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 3, env);
        auto pageIdx = resolveIndex(lhs->children[3].get(), *var, 2, 3, env);
        var->indexDelete3D(rowIdx.data(), rowIdx.size(),
                           colIdx.data(), colIdx.size(),
                           pageIdx.data(), pageIdx.size(),
                           &engine_.allocator_);
    }
    return MValue::empty();
}

// ============================================================
MValue TreeWalker::execBinaryOp(const ASTNode *node, Environment *env)
{
    const std::string &op = node->strValue;

    if (op == "&&") {
        auto l = execNode(node->children[0].get(), env);
        if (!l.toBool())
            return MValue::logicalScalar(false, &engine_.allocator_);
        return MValue::logicalScalar(execNode(node->children[1].get(), env).toBool(),
                                     &engine_.allocator_);
    }
    if (op == "||") {
        auto l = execNode(node->children[0].get(), env);
        if (l.toBool())
            return MValue::logicalScalar(true, &engine_.allocator_);
        return MValue::logicalScalar(execNode(node->children[1].get(), env).toBool(),
                                     &engine_.allocator_);
    }

    auto left = execNode(node->children[0].get(), env);
    auto right = execNode(node->children[1].get(), env);

    // Use cached function pointer if available
    if (node->cachedOp) {
        return (*static_cast<const BinaryOpFunc *>(node->cachedOp))(left, right);
    }

    auto it = engine_.binaryOps_.find(op);
    if (it != engine_.binaryOps_.end()) {
        node->cachedOp = &it->second;
        return it->second(left, right);
    }

    throw std::runtime_error("Undefined binary operator: " + op);
}

MValue TreeWalker::execUnaryOp(const ASTNode *node, Environment *env)
{
    auto operand = execNode(node->children[0].get(), env);

    if (node->cachedOp) {
        return (*static_cast<const UnaryOpFunc *>(node->cachedOp))(operand);
    }

    auto it = engine_.unaryOps_.find(node->strValue);
    if (it != engine_.unaryOps_.end()) {
        node->cachedOp = &it->second;
        return it->second(operand);
    }
    throw std::runtime_error("Undefined unary operator: " + node->strValue);
}

// ============================================================
MValue TreeWalker::callFuncHandle(const MValue &handle, Span<const MValue> args, Environment *env)
{
    auto results = callFuncHandleMulti(handle, args, env, 1);
    return results.empty() ? MValue::empty() : results[0];
}

std::vector<MValue> TreeWalker::callFuncHandleMulti(const MValue &handle,
                                                    Span<const MValue> args,
                                                    Environment *env,
                                                    size_t nout)
{
    const std::string &name = handle.funcHandleName();
    if (engine_.externalFuncs_.count(name)) {
        std::vector<MValue> outBuf(nout);
        CallContext ctx{&engine_, env};
        engine_.externalFuncs_[name](args, nout, Span<MValue>(outBuf), ctx);
        return outBuf;
    }
    {
        auto _uit = engine_.userFuncs_.find(name);
        if (_uit != engine_.userFuncs_.end())
            return callUserFunctionMulti(_uit->second, args, env, nout);
    }
    throw std::runtime_error("Undefined function in handle: @" + name);
}

MValue TreeWalker::execCall(const ASTNode *node, Environment *env, size_t nargout)
{
    auto *funcNode = node->children[0].get();

    if (funcNode->type != NodeType::IDENTIFIER) {
        auto target = execNode(funcNode, env);

        if (target.isFuncHandle()) {
            std::vector<MValue> args;
            args.reserve(node->children.size() - 1);
            for (size_t i = 1; i < node->children.size(); ++i)
                args.push_back(execNode(node->children[i].get(), env));
            return callFuncHandle(target, args, env);
        }

        if (target.isNumeric() || target.isLogical() || target.isChar() || target.isCell())
            return execIndexAccess(target, node, env);

        throw std::runtime_error("Cannot call or index into value of type "
                                 + std::string(mtypeName(target.type())));
    }

    const std::string &name = funcNode->strValue;

    // ── Fast path: cached function pointers (skip env lookup and hash tables) ──
    if (funcNode->cachedUserFunc) {
        MValue argsBuf[4];
        size_t nargs = node->children.size() - 1;
        if (nargs <= 4) {
            for (size_t i = 0; i < nargs; ++i)
                argsBuf[i] = execNode(node->children[i + 1].get(), env);
            return callUserFunction(*static_cast<const UserFunction *>(funcNode->cachedUserFunc),
                                    Span<const MValue>(argsBuf, nargs),
                                    env);
        }
        // >4 args — fall through to vector path
        std::vector<MValue> args;
        args.reserve(nargs);
        for (size_t i = 1; i < node->children.size(); ++i)
            args.push_back(execNode(node->children[i].get(), env));
        return callUserFunction(*static_cast<const UserFunction *>(funcNode->cachedUserFunc),
                                args,
                                env);
    }

    if (funcNode->cachedOp) {
        size_t nargs = node->children.size() - 1;
        MValue argsBuf[4];
        if (nargs <= 4) {
            for (size_t i = 0; i < nargs; ++i)
                argsBuf[i] = execNode(node->children[i + 1].get(), env);
            MValue outBuf[1];
            CallContext ctx{&engine_, env};
            (*static_cast<const ExternalFunc *>(funcNode->cachedOp))(Span<const MValue>(argsBuf,
                                                                                        nargs),
                                                                     1,
                                                                     Span<MValue>(outBuf, 1),
                                                                     ctx);
            return outBuf[0];
        }
    }

    auto buildArgs = [&]() {
        std::vector<MValue> args;
        args.reserve(node->children.size() - 1);
        for (size_t i = 1; i < node->children.size(); ++i)
            args.push_back(execNode(node->children[i].get(), env));
        return args;
    };

    auto *var = env->get(name);
    if (var) {
        if (var->isFuncHandle()) {
            auto args = buildArgs();
            return callFuncHandle(*var, args, env);
        }
        if (var->isNumeric() || var->isLogical() || var->isChar() || var->isCell())
            return execIndexAccess(*var, node, env);
    }

    // Slow path: look up, cache, and call
    {
        auto args = buildArgs();

        if (funcNode->cachedOp) {
            MValue outBuf[1];
            CallContext ctx{&engine_, env};
            (*static_cast<const ExternalFunc *>(
                funcNode->cachedOp))(args, nargout, Span<MValue>(outBuf, 1), ctx);
            return outBuf[0];
        }

        // Slow path: look up and cache
        auto it = engine_.externalFuncs_.find(name);
        if (it != engine_.externalFuncs_.end()) {
            funcNode->cachedOp = &it->second;
            MValue outBuf[1];
            CallContext ctx{&engine_, env};
            it->second(args, nargout, Span<MValue>(outBuf, 1), ctx);
            return outBuf[0];
        }
        {
            auto uit = engine_.userFuncs_.find(name);
            if (uit != engine_.userFuncs_.end()) {
                funcNode->cachedUserFunc = &uit->second;
                return callUserFunction(uit->second, args, env);
            }
        }
    }

    if (var) {
        throw std::runtime_error("Cannot index into variable '" + name + "' of type "
                                 + std::string(mtypeName(var->type())) + ", and no function '"
                                 + name + "' was found");
    }
    throw std::runtime_error("Undefined function or variable: " + name);
}

MValue TreeWalker::execIndexAccess(const MValue &var, const ASTNode *callNode, Environment *env)
{
    size_t nargs = callNode->children.size() - 1;

    auto checkBounds = [](const std::vector<size_t> &indices, size_t limit, const char *ctx) {
        for (auto idx : indices) {
            if (idx >= limit)
                throw std::runtime_error(std::string("Index exceeds array dimensions (") + ctx
                                         + ": " + std::to_string(idx + 1) + " > "
                                         + std::to_string(limit) + ")");
        }
    };

    if (var.isChar()) {
        if (nargs == 1) {
            auto indices = resolveIndex(callNode->children[1].get(), var, 0, 1, env);
            const char *cd = var.charData();
            std::string result;
            result.reserve(indices.size());
            for (auto idx : indices) {
                if (idx >= var.numel())
                    throw std::runtime_error("Index exceeds char array dimensions");
                result += cd[idx];
            }
            return MValue::fromString(result, &engine_.allocator_);
        }
        throw std::runtime_error("Multi-dimensional char indexing not supported");
    }

    if (nargs == 1) {
        // ── scalar fast path: skip resolveIndex + vector<size_t> entirely ──
        size_t scalarIdx;
        if (tryResolveScalarIndex(callNode->children[1].get(), var, 0, 1, env, scalarIdx)) {
            if (scalarIdx >= var.numel())
                throw std::runtime_error("Index exceeds array dimensions (linear index: "
                                         + std::to_string(scalarIdx + 1) + " > "
                                         + std::to_string(var.numel()) + ")");
            // Cell () returns 1×1 sub-cell, not content
            if (var.isCell()) {
                auto result = MValue::cell(1, 1);
                result.cellAt(0) = var.cellAt(scalarIdx);
                return result;
            }
            return var.elemAt(scalarIdx, &engine_.allocator_);
        }

        auto indices = resolveIndex(callNode->children[1].get(), var, 0, 1, env);
        checkBounds(indices, var.numel(), "linear index");
        return var.indexGet(indices.data(), indices.size(), &engine_.allocator_);
    }
    if (nargs == 2) {
        // ── scalar fast path: M(i, j) ──
        {
            size_t sri, sci;
            if (tryResolveScalarIndex(callNode->children[1].get(), var, 0, 2, env, sri)
                && tryResolveScalarIndex(callNode->children[2].get(), var, 1, 2, env, sci)) {
                if (sri >= var.dims().rows())
                    throw std::runtime_error("Index exceeds array dimensions (row index: "
                                             + std::to_string(sri + 1) + " > "
                                             + std::to_string(var.dims().rows()) + ")");
                if (sci >= var.dims().cols())
                    throw std::runtime_error("Index exceeds array dimensions (column index: "
                                             + std::to_string(sci + 1) + " > "
                                             + std::to_string(var.dims().cols()) + ")");
                // Cell () returns 1×1 sub-cell
                if (var.isCell()) {
                    auto result = MValue::cell(1, 1);
                    result.cellAt(0) = var.cellAt(var.dims().sub2ind(sri, sci));
                    return result;
                }
                return var.elemAt(var.dims().sub2ind(sri, sci), &engine_.allocator_);
            }
        }

        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 2, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 2, env);
        checkBounds(ri, var.dims().rows(), "row index");
        checkBounds(ci, var.dims().cols(), "column index");
        return var.indexGet2D(ri.data(), ri.size(), ci.data(), ci.size(), &engine_.allocator_);
    }
    if (nargs == 3) {
        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 3, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 3, env);
        auto pi = resolveIndex(callNode->children[3].get(), var, 2, 3, env);
        return var.indexGet3D(ri.data(), ri.size(), ci.data(), ci.size(),
                             pi.data(), pi.size(), &engine_.allocator_);
    }
    throw std::runtime_error("Too many indexing dimensions (max 3)");
}

// ============================================================
MValue TreeWalker::execCellIndex(const ASTNode *node, Environment *env)
{
    auto obj = execNode(node->children[0].get(), env);
    if (!obj.isCell())
        throw std::runtime_error("Cell indexing {}-operator requires a cell array");

    size_t nidx = node->children.size() - 1;

    if (nidx == 1) {
        IndexContextGuard guard(indexContextStack_, {&obj, 0, 1});
        MValue idx = execNode(node->children[1].get(), env);
        return obj.cellAt(static_cast<size_t>(idx.toScalar()) - 1);
    }
    if (nidx == 2) {
        MValue ridx, cidx;
        {
            IndexContextGuard guard(indexContextStack_, {&obj, 0, 2});
            ridx = execNode(node->children[1].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {&obj, 1, 2});
            cidx = execNode(node->children[2].get(), env);
        }
        size_t r = static_cast<size_t>(ridx.toScalar()) - 1;
        size_t c = static_cast<size_t>(cidx.toScalar()) - 1;
        return obj.cellAt(obj.dims().sub2indChecked(r, c));
    }
    if (nidx == 3) {
        MValue ridx, cidx, pidx;
        {
            IndexContextGuard guard(indexContextStack_, {&obj, 0, 3});
            ridx = execNode(node->children[1].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {&obj, 1, 3});
            cidx = execNode(node->children[2].get(), env);
        }
        {
            IndexContextGuard guard(indexContextStack_, {&obj, 2, 3});
            pidx = execNode(node->children[3].get(), env);
        }
        size_t r = static_cast<size_t>(ridx.toScalar()) - 1;
        size_t c = static_cast<size_t>(cidx.toScalar()) - 1;
        size_t p = static_cast<size_t>(pidx.toScalar()) - 1;
        return obj.cellAt(obj.dims().sub2indChecked(r, c, p));
    }
    throw std::runtime_error("Cell indexing with more than 3 dimensions not supported");
}

MValue TreeWalker::execFieldAccess(const ASTNode *node, Environment *env)
{
    auto obj = execNode(node->children[0].get(), env);
    if (!obj.isStruct())
        throw std::runtime_error("Dot indexing requires a struct, got "
                                 + std::string(mtypeName(obj.type())));
    if (!obj.hasField(node->strValue))
        throw std::runtime_error("Reference to non-existent field '" + node->strValue + "'");
    return obj.field(node->strValue);
}

// ============================================================
MValue TreeWalker::execMatrixLiteral(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return MValue::empty();

    // ── Fast path: [A, x] or [A, x, y, ...] row vector append ──
    // When appending scalars/vectors to a row vector, use amortized growth.
    if (node->children.size() == 1) {
        auto &rowChildren = node->children[0]->children;
        if (rowChildren.size() >= 2 && rowChildren[0]->type == NodeType::IDENTIFIER) {
            MValue *varPtr = env->get(rowChildren[0]->strValue);
            if (varPtr && varPtr->type() == MType::DOUBLE && varPtr->dims().rows() == 1
                && varPtr->dims().cols() > 0) {
                // Evaluate all appended elements
                std::vector<double> appended;
                bool allDoubles = true;
                for (size_t i = 1; i < rowChildren.size(); ++i) {
                    auto val = execNode(rowChildren[i].get(), env);
                    if (val.isScalar() && val.type() == MType::DOUBLE) {
                        appended.push_back(val.toScalar());
                    } else if (val.type() == MType::DOUBLE && val.dims().rows() == 1) {
                        const double *dd = val.doubleData();
                        for (size_t j = 0; j < val.numel(); ++j)
                            appended.push_back(dd[j]);
                    } else {
                        allDoubles = false;
                        break;
                    }
                }
                if (allDoubles && !appended.empty()) {
                    for (double v : appended)
                        varPtr->appendScalar(v, &engine_.allocator_);
                    return *varPtr;
                }
            }
        }
    }

    // Evaluate all elements per row
    std::vector<std::vector<MValue>> rows;
    bool anyChar = false, allChar = true;

    for (auto &rowNode : node->children) {
        std::vector<MValue> rowElems;
        for (auto &elemNode : rowNode->children) {
            auto val = execNode(elemNode.get(), env);
            if (val.isEmpty())
                continue;
            // Logical scalars are kept as-is — horzcat handles type promotion
            if (val.isChar())
                anyChar = true;
            else
                allChar = false;
            rowElems.push_back(std::move(val));
        }
        if (!rowElems.empty())
            rows.push_back(std::move(rowElems));
    }

    if (rows.empty())
        return MValue::empty();

    // All-char: MATLAB pads shorter strings with spaces in vertical stacking
    if (allChar && anyChar) {
        // horzcat each row → one string per row
        std::vector<std::string> strs;
        size_t maxCols = 0;
        for (auto &rowElems : rows) {
            std::string s;
            for (auto &v : rowElems)
                s += v.toString();
            maxCols = std::max(maxCols, s.size());
            strs.push_back(std::move(s));
        }
        if (strs.size() == 1)
            return MValue::fromString(strs[0], &engine_.allocator_);

        // Build char matrix with space-padding
        size_t totalRows = strs.size();
        auto result = MValue::matrix(totalRows, maxCols, MType::CHAR, &engine_.allocator_);
        char *dst = result.charDataMut();
        std::memset(dst, ' ', totalRows * maxCols);
        for (size_t row = 0; row < totalRows; ++row) {
            const auto &s = strs[row];
            for (size_t c = 0; c < s.size(); ++c)
                dst[c * totalRows + row] = s[c];
        }
        return result;
    }

    // Numeric: horzcat each row, then vertcat all rows
    std::vector<MValue> rowValues;
    rowValues.reserve(rows.size());
    for (auto &rowElems : rows)
        rowValues.push_back(
            MValue::horzcat(rowElems.data(), rowElems.size(), &engine_.allocator_));

    if (rowValues.size() == 1)
        return std::move(rowValues[0]);

    return MValue::vertcat(rowValues.data(), rowValues.size(), &engine_.allocator_);
}

MValue TreeWalker::execCellLiteral(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return MValue::cell(0, 0);

    bool is2D = !node->children.empty() && node->children[0]->type == NodeType::BLOCK;

    if (!is2D) {
        auto cell = MValue::cell(1, node->children.size());
        for (size_t i = 0; i < node->children.size(); ++i)
            cell.cellAt(i) = execNode(node->children[i].get(), env);
        return cell;
    }

    size_t numRows = node->children.size();
    size_t numCols = 0;

    for (auto &rowNode : node->children) {
        size_t cols = rowNode->children.size();
        if (numCols == 0) {
            numCols = cols;
        } else if (cols != numCols) {
            throw std::runtime_error(
                "Dimensions of cell arrays being concatenated are not consistent");
        }
    }

    auto cell = MValue::cell(numRows, numCols);

    for (size_t r = 0; r < numRows; ++r) {
        auto &rowNode = node->children[r];
        for (size_t c = 0; c < numCols; ++c) {
            cell.cellAt(cell.dims().sub2ind(r, c)) = execNode(rowNode->children[c].get(), env);
        }
    }

    return cell;
}

// ============================================================
MValue TreeWalker::execColonExpr(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return MValue::fromString(":", &engine_.allocator_);

    if (node->children.size() == 2) {
        double s = execNode(node->children[0].get(), env).toScalar();
        double e = execNode(node->children[1].get(), env).toScalar();
        return MValue::colonRange(s, e, &engine_.allocator_);
    }

    if (node->children.size() == 3) {
        double s = execNode(node->children[0].get(), env).toScalar();
        double step = execNode(node->children[1].get(), env).toScalar();
        double e = execNode(node->children[2].get(), env).toScalar();
        return MValue::colonRange(s, step, e, &engine_.allocator_);
    }

    return MValue::empty();
}

// ============================================================
MValue TreeWalker::execIf(const ASTNode *node, Environment *env)
{
    for (auto &[cond, body] : node->branches) {
        MValue condM;
        bool taken;
        if (tryEvalFast(cond.get(), env, condM))
            taken = (condM.fastScalarVal() != 0.0);
        else
            taken = execNode(cond.get(), env).toBool();
        if (taken)
            return execNode(body.get(), env);
    }
    if (node->elseBranch)
        return execNode(node->elseBranch.get(), env);
    return MValue::empty();
}

MValue TreeWalker::execFor(const ASTNode *node, Environment *env)
{
    const std::string &varName = node->strValue;
    auto rangeVal = execNode(node->children[0].get(), env);

    // Empty range — body never executes (MATLAB behavior)
    if (rangeVal.isEmpty())
        return MValue::empty();

    if (rangeVal.isCell()) {
        size_t cols = rangeVal.dims().cols();
        size_t rows = rangeVal.dims().rows();
        for (size_t c = 0; c < cols; ++c) {
            if (rows == 1) {
                env->set(varName, rangeVal.cellAt(c));
            } else {
                auto col = MValue::cell(rows, 1);
                for (size_t r = 0; r < rows; ++r)
                    col.cellAt(r) = rangeVal.cellAt(rangeVal.dims().sub2ind(r, c));
                env->set(varName, col);
            }
            execNode(node->children[1].get(), env);
            if (flowSignal_ == FlowSignal::BREAK) {
                flowSignal_ = FlowSignal::NONE;
                break;
            }
            if (flowSignal_ == FlowSignal::CONTINUE) {
                flowSignal_ = FlowSignal::NONE;
                continue;
            }
            if (flowSignal_ == FlowSignal::RETURN)
                return MValue::empty();
        }
        return MValue::empty();
    }

    if (rangeVal.type() == MType::DOUBLE) {
        auto dims = rangeVal.dims();
        if (dims.rows() == 1) {
            // ── Fast path: scalar iteration over row vector ──
            // Set the variable once, then update its value in-place
            // to avoid env->set() hash lookup + MValue creation each iteration.
            const double *src = rangeVal.doubleData();
            env->set(varName, MValue::scalar(0.0, &engine_.allocator_));
            MValue *varPtr = env->get(varName);
            double *slot = varPtr->doubleDataMut();
            for (size_t c = 0; c < dims.cols(); ++c) {
                *slot = src[c];
                execNode(node->children[1].get(), env);
                if (flowSignal_ == FlowSignal::BREAK) {
                    flowSignal_ = FlowSignal::NONE;
                    break;
                }
                if (flowSignal_ == FlowSignal::CONTINUE) {
                    flowSignal_ = FlowSignal::NONE;
                    continue;
                }
                if (flowSignal_ == FlowSignal::RETURN)
                    return MValue::empty();
                // Re-fetch pointer: body may have reassigned the loop variable
                varPtr = env->get(varName);
                if (!varPtr || !varPtr->isScalar() || varPtr->type() != MType::DOUBLE) {
                    // Variable was changed to non-scalar — fall back to slow path
                    for (size_t c2 = c + 1; c2 < dims.cols(); ++c2) {
                        env->set(varName, MValue::scalar(src[c2], &engine_.allocator_));
                        execNode(node->children[1].get(), env);
                        if (flowSignal_ == FlowSignal::BREAK) {
                            flowSignal_ = FlowSignal::NONE;
                            return MValue::empty();
                        }
                        if (flowSignal_ == FlowSignal::CONTINUE) {
                            flowSignal_ = FlowSignal::NONE;
                            continue;
                        }
                        if (flowSignal_ == FlowSignal::RETURN)
                            return MValue::empty();
                    }
                    return MValue::empty();
                }
                slot = varPtr->doubleDataMut();
            }
        } else {
            for (size_t c = 0; c < dims.cols(); ++c) {
                auto col = MValue::matrix(dims.rows(), 1, MType::DOUBLE, &engine_.allocator_);
                double *dst = col.doubleDataMut();
                for (size_t r = 0; r < dims.rows(); ++r)
                    dst[r] = rangeVal(r, c);
                env->set(varName, col);
                execNode(node->children[1].get(), env);
                if (flowSignal_ == FlowSignal::BREAK) {
                    flowSignal_ = FlowSignal::NONE;
                    break;
                }
                if (flowSignal_ == FlowSignal::CONTINUE) {
                    flowSignal_ = FlowSignal::NONE;
                    continue;
                }
                if (flowSignal_ == FlowSignal::RETURN)
                    return MValue::empty();
            }
        }
        return MValue::empty();
    }

    if (rangeVal.isChar()) {
        const char *cd = rangeVal.charData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, MValue::fromString(std::string(1, cd[i]), &engine_.allocator_));
            execNode(node->children[1].get(), env);
            if (flowSignal_ == FlowSignal::BREAK) {
                flowSignal_ = FlowSignal::NONE;
                break;
            }
            if (flowSignal_ == FlowSignal::CONTINUE) {
                flowSignal_ = FlowSignal::NONE;
                continue;
            }
            if (flowSignal_ == FlowSignal::RETURN)
                return MValue::empty();
        }
        return MValue::empty();
    }

    if (rangeVal.isLogical()) {
        const uint8_t *ld = rangeVal.logicalData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, MValue::scalar(static_cast<double>(ld[i]), &engine_.allocator_));
            execNode(node->children[1].get(), env);
            if (flowSignal_ == FlowSignal::BREAK) {
                flowSignal_ = FlowSignal::NONE;
                break;
            }
            if (flowSignal_ == FlowSignal::CONTINUE) {
                flowSignal_ = FlowSignal::NONE;
                continue;
            }
            if (flowSignal_ == FlowSignal::RETURN)
                return MValue::empty();
        }
        return MValue::empty();
    }

    throw std::runtime_error("Unsupported type in for loop: "
                             + std::string(mtypeName(rangeVal.type())));
}

MValue TreeWalker::execWhile(const ASTNode *node, Environment *env)
{
    auto *condNode = node->children[0].get();
    for (;;) {
        MValue condM;
        bool cond;
        if (tryEvalFast(condNode, env, condM))
            cond = (condM.fastScalarVal() != 0.0);
        else
            cond = execNode(condNode, env).toBool();
        if (!cond)
            break;
        execNode(node->children[1].get(), env);
        if (flowSignal_ == FlowSignal::BREAK) {
            flowSignal_ = FlowSignal::NONE;
            break;
        }
        if (flowSignal_ == FlowSignal::CONTINUE) {
            flowSignal_ = FlowSignal::NONE;
            continue;
        }
        if (flowSignal_ == FlowSignal::RETURN)
            return MValue::empty();
    }
    return MValue::empty();
}

MValue TreeWalker::execSwitch(const ASTNode *node, Environment *env)
{
    auto sv = execNode(node->children[0].get(), env);

    for (auto &[ce, body] : node->branches) {
        auto cv = execNode(ce.get(), env);
        bool matched = false;

        // isequal-based matching (MATLAB semantics)
        auto valuesEqual = [](const MValue &a, const MValue &b) -> bool {
            if (a.type() != b.type()) return false;
            if (a.isChar() && b.isChar()) return a.toString() == b.toString();
            if (a.numel() != b.numel()) return false;
            if (a.dims() != b.dims()) return false;
            if (a.isScalar() && b.isScalar()) return a.toScalar() == b.toScalar();
            if (a.type() == MType::DOUBLE) {
                const double *da = a.doubleData(), *db = b.doubleData();
                for (size_t i = 0; i < a.numel(); ++i)
                    if (da[i] != db[i]) return false;
                return true;
            }
            if (a.type() == MType::LOGICAL) {
                const uint8_t *la = a.logicalData(), *lb = b.logicalData();
                for (size_t i = 0; i < a.numel(); ++i)
                    if (la[i] != lb[i]) return false;
                return true;
            }
            return false;
        };

        if (cv.isCell()) {
            for (size_t i = 0; i < cv.numel() && !matched; ++i)
                matched = valuesEqual(sv, cv.cellAt(i));
        } else {
            matched = valuesEqual(sv, cv);
        }

        if (matched)
            return execNode(body.get(), env);
    }

    if (node->elseBranch)
        return execNode(node->elseBranch.get(), env);
    return MValue::empty();
}

// ============================================================
MValue TreeWalker::execFunctionDef(const ASTNode *node, Environment *env)
{
    UserFunction func;
    func.name = node->strValue;
    func.params = node->paramNames;
    func.returns = node->returnNames;
    func.body = std::shared_ptr<const ASTNode>(cloneNode(node->children[0].get()));
    func.closureEnv = nullptr;
    engine_.userFuncs_[func.name] = std::move(func);
    return MValue::empty();
}

MValue TreeWalker::execExprStmt(const ASTNode *node, Environment *env)
{
    auto *child = node->children[0].get();

    // Statement context: dispatch CALL and IDENTIFIER with nargout=0
    MValue val;
    if (child->type == NodeType::CALL)
        val = execCall(child, env, 0);
    else if (child->type == NodeType::IDENTIFIER)
        val = execIdentifier(child, env, 0);
    else
        val = execNode(child, env);

    if (!node->suppressOutput && !val.isEmpty()) {
        env->set("ans", val);
        displayValue("ans", val);
    }
    return val;
}

// ============================================================
MValue TreeWalker::execCommandCall(const ASTNode *node, Environment *env)
{
    const std::string &name = node->strValue;

    std::vector<MValue> args;
    args.reserve(node->children.size());
    for (auto &child : node->children)
        args.push_back(MValue::fromString(child->strValue, &engine_.allocator_));

    // 1. External registered functions (includes workspace builtins)
    MValue result;
    if (engine_.externalFuncs_.count(name)) {
        MValue outBuf[1];
        CallContext ctx{&engine_, env};
        engine_.externalFuncs_[name](args, 0, Span<MValue>(outBuf, 1), ctx);
        result = outBuf[0];
        if (!node->suppressOutput && !result.isEmpty()) {
            env->set("ans", result);
            displayValue("ans", result);
        }
        return result;
    }

    // 3. User functions
    {
        auto _uit = engine_.userFuncs_.find(name);
        if (_uit != engine_.userFuncs_.end()) {
            result = callUserFunction(_uit->second, args, env);
            if (!node->suppressOutput && !result.isEmpty()) {
                env->set("ans", result);
                displayValue("ans", result);
            }
            return result;
        }
    }

    throw std::runtime_error("Undefined function: " + name);
}

// ============================================================
MValue TreeWalker::execAnonFunc(const ASTNode *node, Environment *env)
{
    if (!node->strValue.empty() && node->children.empty())
        return MValue::funcHandle(node->strValue, &engine_.allocator_);

    int id = anonCounter_.fetch_add(1, std::memory_order_relaxed);
    std::string anonName = "__anon_" + std::to_string(id);

    UserFunction uf;
    uf.name = anonName;
    uf.params = node->paramNames;
    uf.returns = {"__result__"};

    auto bodyBlock = std::make_shared<ASTNode>(NodeType::BLOCK);

    auto assignNode = std::make_unique<ASTNode>(NodeType::ASSIGN);
    auto resultId = std::make_unique<ASTNode>(NodeType::IDENTIFIER);
    resultId->strValue = "__result__";
    assignNode->children.push_back(std::move(resultId));
    assignNode->children.push_back(cloneNode(node->children[0].get()));
    assignNode->suppressOutput = true;

    bodyBlock->children.push_back(std::move(assignNode));
    uf.body = std::move(bodyBlock);

    uf.closureEnv = env->snapshot(std::shared_ptr<Environment>(engine_.constantsEnv_.get(),
                                                               [](Environment *) {}),
                                  &engine_.globalStore_);

    engine_.userFuncs_[anonName] = std::move(uf);
    return MValue::funcHandle(anonName, &engine_.allocator_);
}

// ============================================================
MValue TreeWalker::execTryCatch(const ASTNode *node, Environment *env)
{
    try {
        auto result = execNode(node->children[0].get(), env);
        // Propagate flow signals — try/catch doesn't intercept break/continue/return
        if (flowSignal_ != FlowSignal::NONE)
            return result;
        return result;
    } catch (const std::exception &e) {
        if (node->children.size() > 1) {
            if (!node->strValue.empty()) {
                auto err = MValue::structure();
                err.field("message") = MValue::fromString(e.what(), &engine_.allocator_);
                err.field("identifier") = MValue::fromString("MLAB:error", &engine_.allocator_);
                env->set(node->strValue, err);
            }
            return execNode(node->children[1].get(), env);
        }
        return MValue::empty();
    }
}

// ============================================================
MValue TreeWalker::execGlobalPersistent(const ASTNode *node, Environment *env)
{
    for (auto &name : node->paramNames) {
        env->declareGlobal(name);
        if (!engine_.globalStore_.get(name))
            engine_.globalStore_.set(name, MValue::empty());
    }
    return MValue::empty();
}

static bool astUsesIdentifier(const ASTNode *node, const char *name1, const char *name2)
{
    if (!node)
        return false;
    if (node->type == NodeType::IDENTIFIER && (node->strValue == name1 || node->strValue == name2))
        return true;
    for (auto &c : node->children)
        if (astUsesIdentifier(c.get(), name1, name2))
            return true;
    for (auto &[cond, body] : node->branches) {
        if (astUsesIdentifier(cond.get(), name1, name2))
            return true;
        if (astUsesIdentifier(body.get(), name1, name2))
            return true;
    }
    if (node->elseBranch && astUsesIdentifier(node->elseBranch.get(), name1, name2))
        return true;
    return false;
}

MValue TreeWalker::callUserFunction(const UserFunction &func,
                                    Span<const MValue> args,
                                    Environment *env)
{
    RecursionGuard rguard(currentRecursionDepth_, maxRecursionDepth_);

    if (args.size() > func.params.size())
        throw std::runtime_error("Too many input arguments for function '" + func.name + "'");

    // Regular functions: parent = constantsEnv (see pi/eps/inf but NOT global variables)
    // Closures: parent = captured scope (already contains correct chain)
    Environment *parentEnv = func.closureEnv ? func.closureEnv.get()
                                             : &engine_.constantsEnvironment();
    Environment localEnv(parentEnv, &engine_.globalStore_);

    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i)
        localEnv.setLocal(func.params[i], args[i]);

    // Lazy: only set nargin/nargout if the function body actually references them
    if (func.usesNarginNargout == -1)
        func.usesNarginNargout = astUsesIdentifier(func.body.get(), "nargin", "nargout") ? 1 : 0;
    if (func.usesNarginNargout) {
        size_t nout = std::max(func.returns.size(), size_t(1));
        localEnv.setLocal("nargin",
                          MValue::scalar(static_cast<double>(args.size()), &engine_.allocator_));
        localEnv.setLocal("nargout", MValue::scalar(static_cast<double>(nout), &engine_.allocator_));
    }

    std::optional<DebugController::FrameGuard> dbgFrame;
    if (auto *ctl = debugCtl()) {
        StackFrame frame;
        frame.functionName = func.name;
        frame.env = &localEnv;
        dbgFrame.emplace(*ctl, std::move(frame));
    }

    execNode(func.body.get(), &localEnv);

    dbgFrame.reset();

    if (flowSignal_ == FlowSignal::RETURN)
        flowSignal_ = FlowSignal::NONE;

    if (func.returns.empty())
        return MValue::empty();

    auto *val = localEnv.getLocal(func.returns[0]);
    if (!val)
        val = localEnv.get(func.returns[0]);
    return val ? std::move(*val) : MValue::empty();
}

std::vector<MValue> TreeWalker::callUserFunctionMulti(const UserFunction &func,
                                                      Span<const MValue> args,
                                                      Environment *env,
                                                      size_t nout)
{
    RecursionGuard rguard(currentRecursionDepth_, maxRecursionDepth_);

    if (args.size() > func.params.size())
        throw std::runtime_error("Too many input arguments for function '" + func.name + "'");

    Environment *parentEnv = func.closureEnv ? func.closureEnv.get()
                                             : &engine_.constantsEnvironment();
    Environment localEnv(parentEnv, &engine_.globalStore_);

    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i)
        localEnv.setLocal(func.params[i], args[i]);

    if (func.usesNarginNargout == -1)
        func.usesNarginNargout = astUsesIdentifier(func.body.get(), "nargin", "nargout") ? 1 : 0;
    if (func.usesNarginNargout) {
        localEnv.setLocal("nargin",
                          MValue::scalar(static_cast<double>(args.size()), &engine_.allocator_));
        localEnv.setLocal("nargout", MValue::scalar(static_cast<double>(nout), &engine_.allocator_));
    }

    for (auto &retName : func.returns)
        if (!localEnv.getLocal(retName))
            localEnv.setLocal(retName, MValue::empty());

    std::optional<DebugController::FrameGuard> dbgFrame;
    if (auto *ctl = debugCtl()) {
        StackFrame frame;
        frame.functionName = func.name;
        frame.env = &localEnv;
        dbgFrame.emplace(*ctl, std::move(frame));
    }

    execNode(func.body.get(), &localEnv);

    dbgFrame.reset();

    if (flowSignal_ == FlowSignal::RETURN)
        flowSignal_ = FlowSignal::NONE;

    std::vector<MValue> results;
    results.reserve(std::min(func.returns.size(), nout));
    for (size_t i = 0; i < func.returns.size() && i < nout; ++i) {
        auto *val = localEnv.getLocal(func.returns[i]);
        if (!val)
            val = localEnv.get(func.returns[i]);
        results.push_back(val ? std::move(*val) : MValue::empty());
    }
    return results;
}

// ============================================================
// Debugger helpers
// ============================================================

DebugController *TreeWalker::debugCtl()
{
    return engine_.debugController_.get();
}

} // namespace mlab