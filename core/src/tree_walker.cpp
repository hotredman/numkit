// src/tree_walker.cpp
#include <numkit/core/tree_walker.hpp>
#include <numkit/core/compiler.hpp>
#include <numkit/core/engine.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace numkit {

TreeWalker::TreeWalker(Engine &engine)
    : engine_(engine)
{}

Value TreeWalker::execute(const ASTNode *ast, Environment *env)
{
    // Pre-scan: register all top-level function definitions before executing
    // the main body.  MATLAB makes local functions available to the entire
    // script regardless of where they appear in the file.
    if (ast && ast->type == NodeType::BLOCK) {
        for (auto &child : ast->children) {
            if (child && child->type == NodeType::FUNCTION_DEF)
                execFunctionDef(child.get(), env);
        }
    }

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
    return engine_.externalFuncs_.count(name) || engine_.hasUserFunction(name)
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

void TreeWalker::displayValue(const std::string &name, const Value &val)
{
    if (val.isObject()) {
        output(engine_.formatObjectDisplay(name, val));
        return;
    }
    output(val.formatDisplay(name));
}

// Returns true and sets `outIdx` if the index expression evaluates to a scalar integer.
bool TreeWalker::tryResolveScalarIndex(const ASTNode *indexExpr,
                                       const Value &array,
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
        if (val && val->isScalar() && val->type() == ValueType::DOUBLE) {
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
            if (val && val->isScalar() && val->type() == ValueType::DOUBLE) {
                lv = val->toScalar();
                lOk = true;
            }
        }
        if (right->type == NodeType::NUMBER_LITERAL) {
            rv = right->numValue;
            rOk = true;
        } else if (right->type == NodeType::IDENTIFIER) {
            auto *val = env->get(right->strValue);
            if (val && val->isScalar() && val->type() == ValueType::DOUBLE) {
                rv = val->toScalar();
                rOk = true;
            }
        }

        if (lOk && rOk) {
            Value lm = Value::scalar(lv, engine_.mr_);
            Value rm = Value::scalar(rv, engine_.mr_);
            Value result = (*static_cast<const BinaryOpFunc *>(indexExpr->cachedOp))(lm, rm);
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
    const ASTNode *indexExpr, const Value &array, int dim, int ndims, Environment *env)
{
    IndexContextGuard guard(indexContextStack_, {&array, dim, ndims});

    Value val = execNode(indexExpr, env);

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

    if (val.type() == ValueType::DOUBLE) {
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
Value TreeWalker::execNodeInner(const ASTNode *node, Environment *env)
{
    switch (node->type) {
    case NodeType::BLOCK:
        return execBlock(node, env);
    case NodeType::NUMBER_LITERAL:
        return Value::scalar(node->numValue, engine_.mr_);
    case NodeType::STRING_LITERAL:
        return Value::fromString(node->strValue, engine_.mr_);
    case NodeType::DQSTRING_LITERAL:
        return Value::stringScalar(node->strValue, engine_.mr_);
    case NodeType::BOOL_LITERAL:
        return Value::logicalScalar(node->boolValue, engine_.mr_);
    case NodeType::IMAG_LITERAL:
        return Value::complexScalar(0.0, node->numValue, engine_.mr_);
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
        return Value();
    case NodeType::CONTINUE_STMT:
        flowSignal_ = FlowSignal::CONTINUE;
        return Value();
    case NodeType::RETURN_STMT:
        flowSignal_ = FlowSignal::RETURN;
        return Value();
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
            return Value::scalar(static_cast<double>(sz), engine_.mr_);
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

Value TreeWalker::execNode(const ASTNode *node, Environment *env)
{
    if (!node)
        return Value();

    try {
        return execNodeInner(node, env);
    } catch (Error &e) {
        // Enrich with this node's source location if the inner throw
        // didn't already attach one (e.g. from a public C++ library API).
        if (node->line > 0)
            e.attachIfMissing(node->line, node->col, "", describeNode(node));
        throw;
    } catch (const DebugStopException &) {
        throw;
    } catch (const std::runtime_error &e) {
        if (node->line > 0)
            throw Error(e.what(), node->line, node->col, "", describeNode(node));
        throw;
    }
}

// Returns true and sets `out` on success, false on failure (caller falls back to execNode).
// ============================================================
// tryEvalFast — fast-path evaluation returning Value
//
// Handles double scalars, logical scalars, and comparison ops
// without falling through to the full execNode path.
// Returns true if the expression was evaluated successfully.
// ============================================================

// Helper: extract a double scalar from an Value (double or logical)
static inline bool asDouble(const Value &v, double &d)
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

bool TreeWalker::tryEvalFast(const ASTNode *expr, Environment *env, Value &out)
{
    switch (expr->type) {
    case NodeType::NUMBER_LITERAL:
        out.setScalarFast(expr->numValue);
        return true;

    case NodeType::IDENTIFIER: {
        Value *v = env->get(expr->strValue);
        if (v && v->isScalar()) {
            ValueType t = v->type();
            if (t == ValueType::DOUBLE) {
                out.setScalarFast(v->toScalar());
                return true;
            }
            if (t == ValueType::LOGICAL) {
                out.setLogicalFast(v->toBool());
                return true;
            }
        }
        return false;
    }

    case NodeType::FIELD_ACCESS: {
        // p.x where p is a struct and p.x is a scalar double/logical
        if (expr->children.size() == 1 && expr->children[0]->type == NodeType::IDENTIFIER) {
            Value *obj = env->get(expr->children[0]->strValue);
            if (obj && obj->isStruct() && obj->hasField(expr->strValue)) {
                const Value &fv = obj->field(expr->strValue);
                if (fv.isScalar()) {
                    ValueType t = fv.type();
                    if (t == ValueType::DOUBLE) {
                        out.setScalarFast(fv.toScalar());
                        return true;
                    }
                    if (t == ValueType::LOGICAL) {
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

        Value lm, rm;
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
        Value lArg = Value::scalar(lv, engine_.mr_);
        Value rArg = Value::scalar(rv, engine_.mr_);
        Value result = (*static_cast<const BinaryOpFunc *>(expr->cachedOp))(lArg, rArg);
        if (result.isScalar()) {
            ValueType t = result.type();
            if (t == ValueType::DOUBLE || t == ValueType::LOGICAL) {
                out = std::move(result);
                return true;
            }
        }
        return false;
    }

    case NodeType::UNARY_OP: {
        if (expr->children.size() != 1)
            return false;
        Value operandM;
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
        Value om = Value::scalar(operand, engine_.mr_);
        Value result = (*static_cast<const UnaryOpFunc *>(expr->cachedOp))(om);
        if (result.isScalar()) {
            ValueType t = result.type();
            if (t == ValueType::DOUBLE || t == ValueType::LOGICAL) {
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
            Value argMs[4];
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
                    // MATLAB: mod(a, 0) == a (std::fmod(a, 0) would be NaN).
                    if (argVals[1] == 0.0) {
                        r = argVals[0];
                    } else {
                        r = std::fmod(argVals[0], argVals[1]);
                        if (r != 0 && ((r < 0) != (argVals[1] < 0)))
                            r += argVals[1];
                    }
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
                // log of a negative promotes to complex — defer to the full
                // builtin (mirrors the sqrt guard above).
                if (nargs == 1 && argVals[0] >= 0) {
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
                if (nargs == 1 && argVals[0] >= 0) {
                    r = std::log2(argVals[0]);
                    ok = true;
                }
                break;
            case 17:
                if (nargs == 1 && argVals[0] >= 0) {
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
                    Value idxM;
                    if (tryEvalFast(expr->children[1].get(), env, idxM)) {
                        double idxVal;
                        if (asDouble(idxM, idxVal)) {
                            Value *arr = env->get(funcNode->strValue);
                            if (arr && arr->type() == ValueType::DOUBLE) {
                                size_t idx = static_cast<size_t>(idxVal) - 1;
                                if (idx < arr->numel()) {
                                    out.setScalarFast(arr->doubleData()[idx]);
                                    return true;
                                }
                            }
                        }
                    }
                } else if (nargs == 2) {
                    Value rowM, colM;
                    if (tryEvalFast(expr->children[1].get(), env, rowM)
                        && tryEvalFast(expr->children[2].get(), env, colM)) {
                        double rowVal, colVal;
                        if (asDouble(rowM, rowVal) && asDouble(colM, colVal)) {
                            Value *arr = env->get(funcNode->strValue);
                            if (arr && arr->type() == ValueType::DOUBLE) {
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
        Value argMs[4];
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
            callArgsBuf_.push_back(Value::scalar(argVals[i], engine_.mr_));
        Value outBuf[1];
        CallContext ctx{&engine_, env};
        (*static_cast<const ExternalFunc *>(
            funcNode->cachedOp))(callArgsBuf_, 1, Span<Value>(outBuf, 1), ctx);
        if (outBuf[0].isScalar()) {
            ValueType t = outBuf[0].type();
            if (t == ValueType::DOUBLE || t == ValueType::LOGICAL) {
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

Value TreeWalker::execBlock(const ASTNode *node, Environment *env)
{
    Value last = Value();
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
                Value fastVal;
                if (tryEvalFast(rhsNode, env, fastVal)) {
                    if (fastVal.isDoubleScalar()) {
                        // Double scalar — try in-place update
                        double dv = fastVal.scalarVal();
                        Value *existing = env->getLocal(lhsName);
                        if (existing && existing->isDoubleScalar()) {
                            existing->setScalarVal(dv);
                            last = *existing;
                        } else {
                            env->set(lhsName, Value::scalar(dv, engine_.mr_));
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
                    Value *arr = env->get(rhsNode->children[0]->strValue);
                    if (arr && arr->type() == ValueType::DOUBLE) {
                        Value idxM;
                        if (tryEvalFast(rhsNode->children[1].get(), env, idxM)) {
                            double idxVal;
                            if (asDouble(idxM, idxVal)) {
                                size_t idx = static_cast<size_t>(idxVal) - 1;
                                if (idx < arr->numel()) {
                                    double v = arr->doubleData()[idx];
                                    Value *existing = env->getLocal(lhsName);
                                    if (existing && existing->isDoubleScalar()) {
                                        existing->setScalarVal(v);
                                    } else {
                                        env->set(lhsName, Value::scalar(v, engine_.mr_));
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
                Value *arr = env->get(arrName);

                if (arr && arr->type() == ValueType::DOUBLE) {
                    if (lhsArgs == 2) {
                        // 1D: A(i) = val
                        Value idxM, rhsM;
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
                                    arr->ensureSize(idx, engine_.mr_);
                                    arr->doubleDataMut()[idx] = rhsVal;
                                }
                                continue;
                            }
                        }
                    } else if (lhsArgs == 3) {
                        // 2D: A(i,j) = val
                        Value rowM, colM, rhsM;
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

            // p.x = <scalar> — struct field scalar assign. Single-
            // struct only; multi-element arrays fall through to
            // execFieldAssign so the broadcast path runs.
            if (lhsNode->type == NodeType::FIELD_ACCESS && lhsNode->children.size() == 1
                && lhsNode->children[0]->type == NodeType::IDENTIFIER) {
                Value fastVal;
                if (tryEvalFast(rhsNode, env, fastVal)) {
                    if (fastVal.isDoubleScalar()) {
                        double dv = fastVal.scalarVal();
                        Value *obj = env->get(lhsNode->children[0]->strValue);
                        if (obj && obj->isStruct() && !obj->isStructArray()) {
                            Value &fv = obj->field(lhsNode->strValue);
                            if (fv.isScalar() && fv.type() == ValueType::DOUBLE) {
                                *fv.doubleDataMut() = dv;
                                continue;
                            }
                            fv = Value::scalar(dv, engine_.mr_);
                            continue;
                        }
                    } else {
                        // Logical or other fast type — set directly
                        Value *obj = env->get(lhsNode->children[0]->strValue);
                        if (obj && obj->isStruct() && !obj->isStructArray()) {
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
Value TreeWalker::execIdentifier(const ASTNode *node, Environment *env, size_t nargout)
{
    const std::string &name = node->strValue;

    auto *val = env->get(name);
    if (val)
        return *val;

    // MATLAB precedence: user-defined functions win over external
    // (builtin) registrations that share a short name. e.g. user defines
    // `function y = square(x)` while `compat.square` is imported — the
    // user function must be called.
    if (auto *_uf = engine_.lookupUserFunction(name, env))
        return callUserFunction(*_uf, {}, env);
    if (const ExternalFunc *fn = engine_.findExternal(name, env)) {
        Value outBuf[1];
        CallContext ctx{&engine_, env};
        (*fn)({}, nargout, Span<Value>(outBuf, 1), ctx);
        return outBuf[0].isEmpty() ? Value() : outBuf[0];
    }

    // MATLAB-exact error for the nargin/nargout pseudo-vars when they're
    // referenced outside of any function scope. Inside a function they are
    // setLocal'd on entry so env->get() above finds them.
    if (name == "nargin" || name == "nargout")
        throw std::runtime_error(
            "You can only call nargin/nargout from within a MATLAB function.");

    throw std::runtime_error("Undefined variable or function: " + name);
}

// ============================================================
Value TreeWalker::execAssign(const ASTNode *node, Environment *env)
{
    auto *lhs = node->children[0].get();
    auto rhs = execNode(node->children[1].get(), env);

    if (lhs->type == NodeType::IDENTIFIER) {
        // Fast path: if variable already exists as a scalar double
        // and rhs is scalar double, write in-place (no hash lookup for set)
        if (rhs.isScalar() && rhs.type() == ValueType::DOUBLE) {
            Value *existing = env->getLocal(lhs->strValue);
            if (existing && existing->isScalar() && existing->type() == ValueType::DOUBLE) {
                *existing->doubleDataMut() = rhs.toScalar();
                if (!node->suppressOutput)
                    displayValue(lhs->strValue, *existing);
                return *existing;
            }
        }
        env->set(lhs->strValue, rhs);
        if (!node->suppressOutput)
            displayValue(lhs->strValue, rhs);
        return rhs;
    }

    // Non-identifier lvalue (a(i)=, s.f=, s.(e)=, c{i}=). Behaviour
    // unchanged: no auto-display for these targets in the TreeWalker.
    assignLValue(lhs, rhs, env);
    return rhs;
}

void TreeWalker::assignLValue(const ASTNode *lhs, const Value &rhs, Environment *env)
{
    if (lhs->type == NodeType::IDENTIFIER) {
        env->set(lhs->strValue, rhs);
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
                env->set(objNode->strValue, Value::structure());
                var = env->get(objNode->strValue);
            }
            if (!var->isStruct())
                *var = Value::structure();
            var->field(fname) = rhs;
        } else {
            throw std::runtime_error("Dynamic field assign: unsupported target");
        }
    } else if (lhs->type == NodeType::CELL_INDEX) {
        execCellAssign(lhs, rhs, env);
    } else {
        throw std::runtime_error("Invalid assignment target");
    }
}

void TreeWalker::execIndexedAssign(const ASTNode *lhs, const Value &rhs, Environment *env)
{
    // The object being indexed may itself be any lvalue: a variable
    // (`a(2)=…`), a struct field (`s.x(2)=…`), a struct-array element
    // field (`d(i).a(2)=…`), or cell content (`c{i}(2)=…`). Resolve it
    // to a mutable slot; the index write below targets that slot. For a
    // bare identifier this is just the variable slot — no copy, hot path
    // unchanged.
    auto *var = &resolveObjectSlot(lhs->children[0].get(), env);

    size_t nargs = lhs->children.size() - 1;

    // OBJECT: obj(i…) = v dispatches to the class subsasgn overload,
    // which mutates `var` in place (value/handle rule via objectStateMut).
    // args = [subscripts…, value].
    if (var->isObject()) {
        const BuiltinClass *cls = engine_.findClass(var->objectClassName());
        if (cls && cls->subsasgn) {
            std::vector<Value> args;
            args.reserve(nargs + 1);
            for (size_t i = 0; i < nargs; ++i)
                args.push_back(execNode(lhs->children[i + 1].get(), env));
            args.push_back(rhs);
            Value outBuf[1];
            CallContext ctx{&engine_, env};
            cls->subsasgn(*var, Span<const Value>(args.data(), args.size()), 0,
                          Span<Value>(outBuf, 1), ctx);
            return;
        }
        // No custom subsasgn → fall through to the builtin object-array
        // element store below (arr(i) = obj).
    }

    // Builtin object-array indexed assignment: arr(i) = obj. Fires when the
    // RHS is an object and the target is empty/unset (→ fresh object array)
    // or an existing object array of the same class. Grows (1-D) with
    // default-constructed gap fill. v1: a single linear index.
    if (rhs.isObject()) {
        const bool newable = !var->isObject() && (var->isUnset() || var->isEmpty());
        const bool sameArr =
            var->isObject() && var->objectClassName() == rhs.objectClassName();
        if (newable || sameArr) {
            if (nargs != 1)
                throw std::runtime_error(
                    "object-array assignment supports a single linear index (v1)");
            auto idxs = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
            if (idxs.size() != 1)
                throw std::runtime_error(
                    "object-array assignment supports a single element (v1)");
            const BuiltinClass *rcls = engine_.findClass(rhs.objectClassName());
            Value fill;
            if (rcls && rcls->construct) {
                CallContext ctx{&engine_, env};
                fill = rcls->construct(Span<const Value>(nullptr, 0), ctx);
            }
            var->objectAssignElement(idxs[0], rhs, fill, engine_.mr_);
            return;
        }
    }

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
                    var->resize(maxIdx, 1, engine_.mr_);
                else
                    var->resize(1, maxIdx, engine_.mr_);
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
                var->ensureSize(scalarIdx, engine_.mr_);
                var->elemSet(scalarIdx, rhs);
                return;
            }
        }

        auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
        for (auto idx : indices)
            var->ensureSize(idx, engine_.mr_);
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
                                engine_.mr_);
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
                        engine_.mr_);

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
                                  engine_.mr_);
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
                          engine_.mr_);

        var->indexSet3D(rowIdx.data(), rowIdx.size(),
                        colIdx.data(), colIdx.size(),
                        pageIdx.data(), pageIdx.size(), rhs);
    } else {
        // ND assign: nargs >= 4. indexSetND auto-grows the target rank
        // (and per-axis size) to fit any out-of-range subscripts.
        const int nd = static_cast<int>(nargs);
        std::vector<std::vector<size_t>> idxLists(nd);
        std::vector<const size_t *> idxPtrs(nd);
        std::vector<size_t> idxCounts(nd);
        for (int i = 0; i < nd; ++i) {
            idxLists[i] = resolveIndex(lhs->children[i + 1].get(), *var, i, nd, env);
            idxPtrs[i] = idxLists[i].data();
            idxCounts[i] = idxLists[i].size();
        }
        var->indexSetND(idxPtrs.data(), idxCounts.data(), nd, rhs);
    }
}

Value &TreeWalker::resolveObjectSlot(const ASTNode *node, Environment *env)
{
    switch (node->type) {
    case NodeType::IDENTIFIER: {
        Value *var = env->get(node->strValue);
        if (!var) {
            env->set(node->strValue, Value());
            var = env->get(node->strValue);
        }
        return *var;
    }
    case NodeType::FIELD_ACCESS:
        return resolveFieldLValue(node, env);
    case NodeType::DYNAMIC_FIELD_ACCESS: {
        std::string fname = execNode(node->children[1].get(), env).toString();
        Value &parent = resolveObjectSlot(node->children[0].get(), env);
        if (!parent.isStruct())
            parent = Value::structure();
        return parent.field(fname);
    }
    case NodeType::CELL_INDEX:
        return resolveCellSlot(node, env);
    default:
        throw std::runtime_error("Invalid assignment target");
    }
}

Value &TreeWalker::resolveCellSlot(const ASTNode *node, Environment *env)
{
    // Resolve the cell container (itself any lvalue), coercing an
    // unset/empty slot to a cell, then return a mutable reference to the
    // addressed content, auto-growing to fit the subscripts (any rank).
    Value &cellVar = resolveObjectSlot(node->children[0].get(), env);
    if (cellVar.isUnset() || cellVar.isEmpty())
        cellVar = Value::cell(0, 0);
    if (!cellVar.isCell())
        throw std::runtime_error("Cell contents indexing on a non-cell value");

    Value *var = &cellVar;
    const size_t nidx = node->children.size() - 1;
    std::vector<size_t> coords(nidx);
    for (size_t i = 0; i < nidx; ++i) {
        IndexContextGuard guard(indexContextStack_,
                                {var, static_cast<int>(i), static_cast<int>(nidx)});
        Value v = execNode(node->children[i + 1].get(), env);
        coords[i] = static_cast<size_t>(v.toScalar()) - 1;
    }
    size_t linear = var->growCellTo(coords.data(), static_cast<int>(nidx), engine_.mr_);
    return var->cellAt(linear);
}

Value &TreeWalker::resolveFieldLValue(const ASTNode *node, Environment *env)
{
    auto *objNode = node->children[0].get();
    const std::string &fieldName = node->strValue;

    if (objNode->type == NodeType::CALL) {
        // d(i).field = val — paren-indexed struct-array element write,
        // any rank, auto-growing to fit (`d(end+1).f`, `d(i,j,k).f`).
        // The indexed object is itself any lvalue (`x.d(i).field`,
        // `c{k}(i).field`, …), resolved to a mutable struct-array slot.
        auto *target = objNode->children[0].get();
        Value *var = &resolveObjectSlot(target, env);
        if (var->isUnset() || var->isEmpty())
            *var = Value::structArray(0, 0, engine_.mr_);
        if (!var->isStruct())
            throw std::runtime_error("Indexed field assignment on a non-struct value");

        const size_t nargs = objNode->children.size() - 1;
        std::vector<size_t> coords(nargs);
        for (size_t a = 0; a < nargs; ++a) {
            IndexContextGuard guard(indexContextStack_,
                                    {var, static_cast<int>(a), static_cast<int>(nargs)});
            Value v = execNode(objNode->children[a + 1].get(), env);
            coords[a] = static_cast<size_t>(v.toScalar()) - 1;
        }
        size_t linear;
        if (nargs == 1) {
            linear = coords[0];
            var->growStructArrayTo(linear, engine_.mr_); // preserves row/col vector shape
        } else {
            linear = var->growStructArrayND(coords.data(), static_cast<int>(nargs),
                                            engine_.mr_);
        }

        auto &fieldMap = var->structArrayElem(linear);
        // BUG #15: track insertion order for fieldnames() before [] auto-creates.
        if (fieldMap.find(fieldName) == fieldMap.end()) {
            if (var->isStruct()) {
                // setField updates fieldOrder for new key; reuse it then return ref.
                var->setField(linear, fieldName, Value{});
            }
        }
        return fieldMap[fieldName];
    }

    // General case: the object is any other addressable lvalue
    // (identifier, nested field, dynamic field, cell content). Resolve
    // it, coerce to a scalar struct, and return the field slot.
    Value &parent = resolveObjectSlot(objNode, env);
    if (!parent.isStruct())
        parent = Value::structure();
    return parent.field(fieldName);
}

void TreeWalker::execFieldAssign(const ASTNode *lhs, const Value &rhs, Environment *env)
{
    // OBJECT: obj.Prop = v sets via the class property hook. Only peek
    // when the parent is a slot resolveObjectSlot can address (a CALL
    // parent is `d(i).field` — a struct-array element write, handled
    // below; object arrays are a later phase). propSet detaches the slot
    // (COW) so the value/handle rule applies and the variable updates.
    {
        const ASTNode *objNode = lhs->children[0].get();
        if (objNode->type == NodeType::IDENTIFIER
            || objNode->type == NodeType::FIELD_ACCESS
            || objNode->type == NodeType::DYNAMIC_FIELD_ACCESS
            || objNode->type == NodeType::CELL_INDEX) {
            Value &parent = resolveObjectSlot(objNode, env);
            if (parent.isObject()) {
                objectPropSet(parent, lhs->strValue, rhs, env);
                return;
            }
        }
    }
    // Broadcast write: `s.f = val` where s is a multi-element struct
    // array sets f on every element. MATLAB semantics. The single-
    // struct path stays the same (resolveFieldLValue throws on multi-
    // element by design — that contract is what we're side-stepping).
    if (lhs->children.size() == 1
        && lhs->children[0]->type == NodeType::IDENTIFIER) {
        auto *var = env->get(lhs->children[0]->strValue);
        if (var && var->isStructArray()) {
            const std::string &fname = lhs->strValue;
            var->setFieldAll(fname, rhs);  // BUG #15: track insertion order
            return;
        }
    }
    resolveFieldLValue(lhs, env) = rhs;
}

void TreeWalker::execCellAssign(const ASTNode *lhs, const Value &rhs, Environment *env)
{
    // c{i} = rhs assigns the addressed content. resolveCellSlot handles
    // container coercion, grow and N-D subscripts for any cell lvalue
    // (`c{i}`, `s.c{i}`, `a.b{i,j}`, …).
    resolveObjectSlot(lhs, env) = rhs;
}

// ============================================================
Value TreeWalker::execMultiAssign(const ASTNode *node, Environment *env)
{
    const size_t nout = node->returnNames.size();
    auto results = execCallMulti(node->children[0].get(), env, nout);

    // MATLAB: requesting more outputs than the RHS produces is an error
    // at the call site ("Too many output arguments"), not a silently
    // unassigned target that later reads as a phantom undefined function
    // (bug #44 — `[c,sz,n2,p] = bwconncomp(A)` where bwconncomp yields
    // one value). An unfilled output slot is the unset sentinel.
    for (size_t i = 0; i < nout; ++i)
        if (i >= results.size() || results[i].isUnset())
            throw std::runtime_error("Too many output arguments.");

    // Complex-target path: at least one output is a general lvalue
    // (`s.f`, `a(i)`, `c{i}`, ...). lhsTargets is authoritative; a
    // nullptr entry is an ignored `~`.
    if (!node->lhsTargets.empty()) {
        for (size_t i = 0; i < node->lhsTargets.size() && i < results.size(); ++i)
            if (node->lhsTargets[i])
                assignLValue(node->lhsTargets[i].get(), results[i], env);

        // Display only bare-identifier targets (mirrors single-assign,
        // where `s.f = v` / `a(i) = v` do not auto-display in the TW).
        if (!node->suppressOutput && !results.empty())
            for (size_t i = 0; i < node->lhsTargets.size() && i < results.size(); ++i) {
                const ASTNode *t = node->lhsTargets[i].get();
                if (t && t->type == NodeType::IDENTIFIER)
                    displayValue(t->strValue, results[i]);
            }
        return results.empty() ? Value() : results[0];
    }

    for (size_t i = 0; i < node->returnNames.size() && i < results.size(); ++i)
        if (node->returnNames[i] != "~")
            env->set(node->returnNames[i], results[i]);

    if (!node->suppressOutput && !results.empty())
        for (size_t i = 0; i < node->returnNames.size() && i < results.size(); ++i)
            if (node->returnNames[i] != "~")
                displayValue(node->returnNames[i], results[i]);

    return results.empty() ? Value() : results[0];
}

std::vector<Value> TreeWalker::execCallMulti(const ASTNode *node, Environment *env, size_t nout)
{
    // Cell CSL: [a,b] = c{idx} — expand cell elements into separate outputs
    if (node->type == NodeType::CELL_INDEX) {
        const Value *cell = env->get(node->children[0]->strValue);
        if (!cell || !cell->isCell())
            throw std::runtime_error("Cell indexing requires a cell array");

        size_t nidx = node->children.size() - 1;
        if (nidx == 1) {
            auto indices = resolveIndex(node->children[1].get(), *cell, 0, 1, env);
            std::vector<Value> out;
            out.reserve(indices.size());
            for (size_t idx : indices)
                out.push_back(cell->cellAt(idx));
            return out;
        } else if (nidx == 2) {
            auto rowIdx = resolveIndex(node->children[1].get(), *cell, 0, 2, env);
            auto colIdx = resolveIndex(node->children[2].get(), *cell, 1, 2, env);
            std::vector<Value> out;
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

    // OBJECT dotted multi-output method: [a,b] = obj.m(args). Peek the
    // identifier-rooted receiver (mirrors execCall's dotted dispatch); a
    // class method returning several outputs fills nout result slots.
    auto *headNode = node->children[0].get();
    if (headNode->type == NodeType::FIELD_ACCESS
        && headNode->children[0]->type == NodeType::IDENTIFIER) {
        Value *objPtr = env->get(headNode->children[0]->strValue);
        if (objPtr && objPtr->isObject()) {
            const std::string &mname = headNode->strValue;
            const BuiltinClass *cls = engine_.findClass(objPtr->objectClassName());
            if (cls && cls->methods.count(mname)) {
                std::vector<Value> margs;
                margs.reserve(node->children.size() - 1);
                for (size_t i = 1; i < node->children.size(); ++i)
                    margs.push_back(execNode(node->children[i].get(), env));
                Value self = *objPtr; // handle: shares state; value: own copy
                std::vector<Value> outBuf(nout);
                CallContext ctx{&engine_, env};
                cls->methods.at(mname)(self, Span<const Value>(margs.data(), margs.size()),
                                       nout, Span<Value>(outBuf), ctx);
                return outBuf;
            }
        }
    }

    const std::string &funcName = node->children[0]->strValue;

    std::vector<Value> args;
    args.reserve(node->children.size() - 1);
    for (size_t i = 1; i < node->children.size(); ++i)
        args.push_back(execNode(node->children[i].get(), env));

    auto *var = env->get(funcName);
    if (var && var->isFuncHandle())
        return callFuncHandleMulti(*var, args, env, nout, node);

    // OBJECT function-form multi-output: [a,b] = m(obj, ...). A class
    // method on the dominant (first) object argument beats a path function.
    if (!args.empty() && args[0].isObject()) {
        const BuiltinClass *cls = engine_.findClass(args[0].objectClassName());
        if (cls && cls->methods.count(funcName)) {
            Value self = args[0];
            std::vector<Value> rest(args.begin() + 1, args.end());
            std::vector<Value> outBuf(nout);
            CallContext ctx{&engine_, env};
            cls->methods.at(funcName)(self, Span<const Value>(rest.data(), rest.size()),
                                      nout, Span<Value>(outBuf), ctx);
            return outBuf;
        }
    }

    // Fast path: cached function pointer
    auto *funcNode = node->children[0].get();
    if (funcNode->cachedOp) {
        std::vector<Value> outBuf(nout);
        CallContext ctx{&engine_, env};
        (*static_cast<const ExternalFunc *>(
            funcNode->cachedOp))(args, nout, Span<Value>(outBuf), ctx);
        return outBuf;
    }

    // User-defined functions take precedence over external builtins
    // sharing the same short name (MATLAB semantics).
    if (auto *_uf = engine_.lookupUserFunction(funcName, env)) {
        funcNode->cachedUserFunc = _uf;
        return callUserFunctionMulti(*_uf, args, env, nout, node);
    }
    if (const ExternalFunc *fn = engine_.findExternal(funcName, env)) {
        funcNode->cachedOp = fn;
        std::vector<Value> outBuf(nout);
        CallContext ctx{&engine_, env};
        (*fn)(args, nout, Span<Value>(outBuf), ctx);
        return outBuf;
    }

    throw std::runtime_error("Undefined function: " + funcName);
}

// ============================================================
Value TreeWalker::execDeleteAssign(const ASTNode *node, Environment *env)
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
        var->indexDelete(indices.data(), indices.size(), engine_.mr_);
    } else if (nargs == 2) {
        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 2, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 2, env);
        var->indexDelete2D(rowIdx.data(), rowIdx.size(),
                           colIdx.data(), colIdx.size(),
                           engine_.mr_);
    } else if (nargs == 3) {
        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 3, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 3, env);
        auto pageIdx = resolveIndex(lhs->children[3].get(), *var, 2, 3, env);
        var->indexDelete3D(rowIdx.data(), rowIdx.size(),
                           colIdx.data(), colIdx.size(),
                           pageIdx.data(), pageIdx.size(),
                           engine_.mr_);
    } else {
        // ND delete: A(i_1, ..., i_n) = []. Resolve every dim's index
        // vector, then dispatch to the generic ND deleter (which checks
        // exactly-one-partial-axis itself).
        std::vector<std::vector<size_t>> perDim(nargs);
        std::vector<const size_t *> perDimPtrs(nargs);
        std::vector<size_t> perDimCount(nargs);
        for (size_t i = 0; i < nargs; ++i) {
            perDim[i] = resolveIndex(lhs->children[i + 1].get(), *var,
                                     static_cast<int>(i), static_cast<int>(nargs), env);
            perDimPtrs[i]  = perDim[i].data();
            perDimCount[i] = perDim[i].size();
        }
        var->indexDeleteND(perDimPtrs.data(), perDimCount.data(),
                           static_cast<int>(nargs), engine_.mr_);
    }
    return Value();
}

// ============================================================
Value TreeWalker::execBinaryOp(const ASTNode *node, Environment *env)
{
    const std::string &op = node->strValue;

    if (op == "&&") {
        auto l = execNode(node->children[0].get(), env);
        if (!l.toBool())
            return Value::logicalScalar(false, engine_.mr_);
        return Value::logicalScalar(execNode(node->children[1].get(), env).toBool(),
                                     engine_.mr_);
    }
    if (op == "||") {
        auto l = execNode(node->children[0].get(), env);
        if (l.toBool())
            return Value::logicalScalar(true, engine_.mr_);
        return Value::logicalScalar(execNode(node->children[1].get(), env).toBool(),
                                     engine_.mr_);
    }

    auto left = execNode(node->children[0].get(), env);
    auto right = execNode(node->children[1].get(), env);

    // OBJECT operator overloading: dispatch to the dominant object's class
    // `ops` before the numeric/cached builtin path. Checked first so a
    // cachedOp from an earlier numeric evaluation of this node can't
    // hijack an object operand (throws if no matching overload exists).
    if (left.isObject() || right.isObject()) {
        Value out;
        if (engine_.tryObjectBinaryOp(op, left, right, env, out))
            return out;
    }

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

Value TreeWalker::execUnaryOp(const ASTNode *node, Environment *env)
{
    auto operand = execNode(node->children[0].get(), env);

    // OBJECT unary operator overloading — before the cached/builtin path.
    if (operand.isObject()) {
        Value out;
        if (engine_.tryObjectUnaryOp(node->strValue, operand, env, out))
            return out;
    }

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
Value TreeWalker::callFuncHandle(const Value &handle, Span<const Value> args, Environment *env,
                                  const ASTNode *callNode)
{
    auto results = callFuncHandleMulti(handle, args, env, 1, callNode);
    return results.empty() ? Value() : results[0];
}

Value TreeWalker::callHandlePublic(const Value &handle,
                                    Span<const Value> args,
                                    Environment *env)
{
    return callFuncHandle(handle, args, env);
}

std::vector<Value>
TreeWalker::callHandleMultiPublic(const Value &handle,
                                  Span<const Value> args,
                                  Environment *env,
                                  size_t nout)
{
    return callFuncHandleMulti(handle, args, env, nout);
}

std::vector<Value> TreeWalker::callFuncHandleMulti(const Value &handle,
                                                    Span<const Value> args,
                                                    Environment *env,
                                                    size_t nout,
                                                    const ASTNode *callNode)
{
    const std::string &name = handle.funcHandleName();
    if (engine_.externalFuncs_.count(name)) {
        std::vector<Value> outBuf(nout);
        CallContext ctx{&engine_, env};
        engine_.externalFuncs_[name](args, nout, Span<Value>(outBuf), ctx);
        return outBuf;
    }
    if (auto *_uf = engine_.lookupUserFunction(name, env))
        return callUserFunctionMulti(*_uf, args, env, nout, callNode);
    throw std::runtime_error("Undefined function in handle: @" + name);
}

Value TreeWalker::execCall(const ASTNode *node, Environment *env, size_t nargout)
{
    auto *funcNode = node->children[0].get();

    if (funcNode->type != NodeType::IDENTIFIER) {
        // Qualified-name call: a chain of FIELD_ACCESS over a root
        // IDENTIFIER (`pkg.foo(x)`, `pkg.sub.bar(x)`). When the root
        // identifier is NOT a workspace variable, try resolving the
        // dotted name as a namespace member (registered builtin or
        // +pkg/.../<leaf>.m). A real struct variable shadows this —
        // the existing FIELD_ACCESS path then handles `s.field(x)`.
        const ASTNode *rootIdent = nullptr;
        std::string qualified = tryBuildQualifiedName(funcNode, &rootIdent);
        if (!qualified.empty() && !env->get(rootIdent->strValue)) {
            std::vector<Value> args;
            args.reserve(node->children.size() - 1);
            for (size_t i = 1; i < node->children.size(); ++i)
                args.push_back(execNode(node->children[i].get(), env));
            // OBJECT: package-qualified constructor (`containers.Map(...)`).
            if (const BuiltinClass *cls = engine_.findClass(qualified);
                cls && cls->construct) {
                CallContext ctx{&engine_, env};
                return cls->construct(Span<const Value>(args.data(), args.size()), ctx);
            }
            // User-defined first (MATLAB precedence).
            if (auto *uf = engine_.lookupUserFunction(qualified, env))
                return callUserFunction(*uf, args, env, node);
            if (const ExternalFunc *fn = engine_.findExternal(qualified, env)) {
                Value outBuf[1];
                CallContext ctx{&engine_, env};
                (*fn)(args, nargout, Span<Value>(outBuf, 1), ctx);
                return outBuf[0];
            }
            throw std::runtime_error("Undefined function or variable: " + qualified);
        }

        // OBJECT: obj.method(args) (dotted method) or obj.prop(idx)
        // (property then index). Peek the receiver via env->get so a
        // method name isn't mis-read as a missing property. Limited to
        // identifier-rooted receivers (the common case; no re-eval /
        // double side effects).
        if (funcNode->type == NodeType::FIELD_ACCESS
            && funcNode->children[0]->type == NodeType::IDENTIFIER) {
            Value *objPtr = env->get(funcNode->children[0]->strValue);
            if (objPtr && objPtr->isObject()) {
                const std::string &mname = funcNode->strValue;
                const BuiltinClass *cls = engine_.findClass(objPtr->objectClassName());
                if (cls && cls->methods.count(mname)) {
                    std::vector<Value> args;
                    args.reserve(node->children.size() - 1);
                    for (size_t i = 1; i < node->children.size(); ++i)
                        args.push_back(execNode(node->children[i].get(), env));
                    Value self = *objPtr; // handle: shares state; value: own copy
                    Value outBuf[1];
                    CallContext ctx{&engine_, env};
                    cls->methods.at(mname)(self, Span<const Value>(args.data(), args.size()),
                                           nargout, Span<Value>(outBuf, 1), ctx);
                    return outBuf[0];
                }
                // Not a method → property read, then index with the args.
                Value prop = objectPropGet(*objPtr, mname, env);
                return execIndexAccess(prop, node, env);
            }
        }

        auto target = execNode(funcNode, env);

        if (target.isFuncHandle()) {
            std::vector<Value> args;
            args.reserve(node->children.size() - 1);
            for (size_t i = 1; i < node->children.size(); ++i)
                args.push_back(execNode(node->children[i].get(), env));
            return callFuncHandle(target, args, env, node);
        }

        if (target.isNumeric() || target.isLogical() || target.isChar()
            || target.isCell() || target.isStruct() || target.isString())
            return execIndexAccess(target, node, env);

        throw std::runtime_error("Cannot call or index into value of type "
                                 + std::string(mtypeName(target.type())));
    }

    const std::string &name = funcNode->strValue;

    // ── Fast path: cached function pointers (skip env lookup and hash tables) ──
    if (funcNode->cachedUserFunc) {
        Value argsBuf[4];
        size_t nargs = node->children.size() - 1;
        if (nargs <= 4) {
            for (size_t i = 0; i < nargs; ++i)
                argsBuf[i] = execNode(node->children[i + 1].get(), env);
            return callUserFunction(*static_cast<const UserFunction *>(funcNode->cachedUserFunc),
                                    Span<const Value>(argsBuf, nargs),
                                    env, node);
        }
        // >4 args — fall through to vector path
        std::vector<Value> args;
        args.reserve(nargs);
        for (size_t i = 1; i < node->children.size(); ++i)
            args.push_back(execNode(node->children[i].get(), env));
        return callUserFunction(*static_cast<const UserFunction *>(funcNode->cachedUserFunc),
                                args,
                                env, node);
    }

    if (funcNode->cachedOp) {
        size_t nargs = node->children.size() - 1;
        Value argsBuf[4];
        if (nargs <= 4) {
            for (size_t i = 0; i < nargs; ++i)
                argsBuf[i] = execNode(node->children[i + 1].get(), env);
            Value outBuf[1];
            CallContext ctx{&engine_, env};
            (*static_cast<const ExternalFunc *>(funcNode->cachedOp))(Span<const Value>(argsBuf,
                                                                                        nargs),
                                                                     1,
                                                                     Span<Value>(outBuf, 1),
                                                                     ctx);
            return outBuf[0];
        }
    }

    auto buildArgs = [&]() {
        std::vector<Value> args;
        args.reserve(node->children.size() - 1);
        for (size_t i = 1; i < node->children.size(); ++i)
            args.push_back(execNode(node->children[i].get(), env));
        return args;
    };

    auto *var = env->get(name);
    if (var) {
        if (var->isFuncHandle()) {
            auto args = buildArgs();
            return callFuncHandle(*var, args, env, node);
        }
        // OBJECT: obj(i…) dispatches to the class subsref overload.
        if (var->isObject()) {
            const BuiltinClass *cls = engine_.findClass(var->objectClassName());
            if (cls && cls->subsref) {
                auto args = buildArgs();
                Value self = *var;
                Value outBuf[1];
                CallContext ctx{&engine_, env};
                cls->subsref(self, Span<const Value>(args.data(), args.size()), nargout,
                             Span<Value>(outBuf, 1), ctx);
                return outBuf[0];
            }
            // No custom subsref → builtin object-array indexing: obj(i)
            // selects element(s) (a scalar object is a 1×1 array). `end`
            // binds to numel via resolveIndex over the object array.
            if (node->children.size() == 2) {
                auto idxs = resolveIndex(node->children[1].get(), *var, 0, 1, env);
                return var->objectSubArray(idxs, engine_.mr_);
            }
            throw std::runtime_error("'()' indexing is not defined for class '"
                                     + var->objectClassName() + "'");
        }
        if (var->isNumeric() || var->isLogical() || var->isChar() || var->isCell()
            || var->isStruct() || var->isString())
            return execIndexAccess(*var, node, env);
    }

    // OBJECT: ClassName(args) constructs an instance when `name` is a
    // registered class not shadowed by a variable (object model §3).
    if (!var) {
        if (const BuiltinClass *cls = engine_.findClass(name); cls && cls->construct) {
            auto args = buildArgs();
            CallContext ctx{&engine_, env};
            return cls->construct(Span<const Value>(args.data(), args.size()), ctx);
        }
    }

    // Slow path: look up, cache, and call
    {
        auto args = buildArgs();

        // OBJECT function-form dispatch: m(obj, ...) where obj's class
        // defines method m beats a same-named global function (MATLAB).
        // Kept in the slow path (object-method calls don't get cached),
        // so the common builtin path stays on its fast cache.
        if (!args.empty() && args[0].isObject()) {
            const BuiltinClass *cls = engine_.findClass(args[0].objectClassName());
            if (cls && cls->methods.count(name)) {
                Value self = args[0];
                std::vector<Value> rest(args.begin() + 1, args.end());
                Value outBuf[1];
                CallContext ctx{&engine_, env};
                cls->methods.at(name)(self, Span<const Value>(rest.data(), rest.size()),
                                      nargout, Span<Value>(outBuf, 1), ctx);
                return outBuf[0];
            }
        }

        if (funcNode->cachedOp) {
            Value outBuf[1];
            CallContext ctx{&engine_, env};
            (*static_cast<const ExternalFunc *>(
                funcNode->cachedOp))(args, nargout, Span<Value>(outBuf, 1), ctx);
            return outBuf[0];
        }

        // User-defined first (MATLAB precedence over imports/builtins).
        if (auto *uf = engine_.lookupUserFunction(name, env)) {
            funcNode->cachedUserFunc = uf;
            return callUserFunction(*uf, args, env, node);
        }
        if (const ExternalFunc *fn = engine_.findExternal(name, env)) {
            funcNode->cachedOp = fn;
            Value outBuf[1];
            CallContext ctx{&engine_, env};
            (*fn)(args, nargout, Span<Value>(outBuf, 1), ctx);
            return outBuf[0];
        }
    }

    if (var) {
        throw std::runtime_error("Cannot index into variable '" + name + "' of type "
                                 + std::string(mtypeName(var->type())) + ", and no function '"
                                 + name + "' was found");
    }
    throw std::runtime_error("Undefined function or variable: " + name);
}

Value TreeWalker::execIndexAccess(const Value &var, const ASTNode *callNode, Environment *env)
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

    // CHAR routes through the generic path below. 1D reads go through
    // Value::indexGet (which has a CHAR case building a char row via
    // fromString); 2D/3D reads use the memcpy path in indexGet2D /
    // indexGet3D (elementSize(CHAR)==1, so the raw byte copy preserves
    // matrix shape). Indexed assignment works via writeElem / writeScalar.

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
                auto result = Value::cell(1, 1);
                result.cellAt(0) = var.cellAt(scalarIdx);
                return result;
            }
            return var.elemAt(scalarIdx, engine_.mr_);
        }

        auto indices = resolveIndex(callNode->children[1].get(), var, 0, 1, env);
        checkBounds(indices, var.numel(), "linear index");
        return var.indexGet(indices.data(), indices.size(), engine_.mr_);
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
                    auto result = Value::cell(1, 1);
                    result.cellAt(0) = var.cellAt(var.dims().sub2ind(sri, sci));
                    return result;
                }
                return var.elemAt(var.dims().sub2ind(sri, sci), engine_.mr_);
            }
        }

        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 2, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 2, env);
        checkBounds(ri, var.dims().rows(), "row index");
        checkBounds(ci, var.dims().cols(), "column index");
        return var.indexGet2D(ri.data(), ri.size(), ci.data(), ci.size(), engine_.mr_);
    }
    if (nargs == 3) {
        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 3, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 3, env);
        auto pi = resolveIndex(callNode->children[3].get(), var, 2, 3, env);
        return var.indexGet3D(ri.data(), ri.size(), ci.data(), ci.size(),
                             pi.data(), pi.size(), engine_.mr_);
    }
    // ND read: nargs >= 4
    {
        const int nd = static_cast<int>(nargs);
        std::vector<std::vector<size_t>> idxLists(nd);
        std::vector<const size_t *> idxPtrs(nd);
        std::vector<size_t> idxCounts(nd);
        for (int i = 0; i < nd; ++i) {
            idxLists[i] = resolveIndex(callNode->children[i + 1].get(), var, i, nd, env);
            idxPtrs[i] = idxLists[i].data();
            idxCounts[i] = idxLists[i].size();
        }
        return var.indexGetND(idxPtrs.data(), idxCounts.data(), nd, engine_.mr_);
    }
}

// ============================================================
Value TreeWalker::execCellIndex(const ASTNode *node, Environment *env)
{
    auto obj = execNode(node->children[0].get(), env);
    if (!obj.isCell())
        throw std::runtime_error("Cell indexing {}-operator requires a cell array");

    size_t nidx = node->children.size() - 1;

    if (nidx == 1) {
        IndexContextGuard guard(indexContextStack_, {&obj, 0, 1});
        Value idx = execNode(node->children[1].get(), env);
        return obj.cellAt(static_cast<size_t>(idx.toScalar()) - 1);
    }
    if (nidx == 2) {
        Value ridx, cidx;
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
        Value ridx, cidx, pidx;
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
    // ND brace-cell read (nidx ≥ 4): column-major linear index, bounds-checked.
    std::vector<size_t> coords(nidx);
    for (size_t i = 0; i < nidx; ++i) {
        IndexContextGuard guard(indexContextStack_, {&obj, static_cast<int>(i),
                                                     static_cast<int>(nidx)});
        Value v = execNode(node->children[i + 1].get(), env);
        coords[i] = static_cast<size_t>(v.toScalar()) - 1;
    }
    const auto &d = obj.dims();
    size_t idx = 0, stride = 1;
    for (size_t i = 0; i < nidx; ++i) {
        const size_t lim = (static_cast<int>(i) < d.ndim()) ? d.dim(static_cast<int>(i)) : 1;
        if (coords[i] >= lim)
            throw std::runtime_error("Cell index out of bounds (dim "
                                     + std::to_string(i + 1)
                                     + ": " + std::to_string(coords[i] + 1)
                                     + " > " + std::to_string(lim) + ")");
        idx += coords[i] * stride;
        stride *= lim;
    }
    return obj.cellAt(idx);
}

Value TreeWalker::execFieldAccess(const ASTNode *node, Environment *env)
{
    auto obj = execNode(node->children[0].get(), env);
    // OBJECT: obj.Prop reads via the class property hook (object model,
    // OBJECT_MODEL.md §3). No-arg method call form is wired in P3.
    if (obj.isObject())
        return objectPropGet(obj, node->strValue, env);
    if (!obj.isStruct())
        throw std::runtime_error("Dot indexing requires a struct, got "
                                 + std::string(mtypeName(obj.type())));
    if (!obj.hasField(node->strValue))
        throw std::runtime_error("Reference to non-existent field '" + node->strValue + "'");
    return obj.field(node->strValue);
}

Value TreeWalker::objectPropGet(const Value &obj, const std::string &name, Environment *env)
{
    const BuiltinClass *cls = engine_.findClass(obj.objectClassName());
    if (cls) {
        CallContext ctx{&engine_, env};
        if (cls->propGet) {
            Value out;
            if (cls->propGet(obj, name, out, ctx))
                return out;
        }
        // Bare `obj.method` (no parens) invokes a no-arg method (MATLAB).
        auto mit = cls->methods.find(name);
        if (mit != cls->methods.end()) {
            Value self = obj;
            Value outBuf[1];
            mit->second(self, Span<const Value>(nullptr, 0), 1, Span<Value>(outBuf, 1), ctx);
            return outBuf[0];
        }
    }
    throw std::runtime_error("No appropriate property '" + name + "' for class '"
                             + obj.objectClassName() + "'");
}

void TreeWalker::objectPropSet(Value &objSlot, const std::string &name,
                               const Value &rhs, Environment *env)
{
    const BuiltinClass *cls = engine_.findClass(objSlot.objectClassName());
    if (cls && cls->propSet) {
        CallContext ctx{&engine_, env};
        if (cls->propSet(objSlot, name, rhs, ctx))
            return;
    }
    throw std::runtime_error("Cannot set property '" + name + "' on class '"
                             + objSlot.objectClassName() + "'");
}

// ============================================================
Value TreeWalker::execMatrixLiteral(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return Value();

    // ── Fast path: [A, x] or [A, x, y, ...] row vector append ──
    // When appending scalars/vectors to a row vector, use amortized growth.
    // CAUTION: this mutates the variable named by rowChildren[0]. Skip if
    // that name is a reserved constant (NaN/Inf/pi/eps/true/false/i/j/...)
    // — otherwise [NaN, 1, 2] would mutate the global NaN constant.
    if (node->children.size() == 1) {
        auto &rowChildren = node->children[0]->children;
        if (rowChildren.size() >= 2 && rowChildren[0]->type == NodeType::IDENTIFIER
            && !engine_.isReservedName(rowChildren[0]->strValue)) {
            Value *varPtr = env->get(rowChildren[0]->strValue);
            if (varPtr && varPtr->type() == ValueType::DOUBLE && varPtr->dims().rows() == 1
                && varPtr->dims().cols() > 0) {
                // Evaluate all appended elements
                std::vector<double> appended;
                bool allDoubles = true;
                for (size_t i = 1; i < rowChildren.size(); ++i) {
                    auto val = execNode(rowChildren[i].get(), env);
                    if (val.isScalar() && val.type() == ValueType::DOUBLE) {
                        appended.push_back(val.toScalar());
                    } else if (val.type() == ValueType::DOUBLE && val.dims().rows() == 1) {
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
                        varPtr->appendScalar(v, engine_.mr_);
                    return *varPtr;
                }
            }
        }
    }

    // Evaluate all elements per row, with comma-separated-list expansion
    // for struct-array dot access (`[d.field]` / `[s.fname]`).
    std::vector<std::vector<Value>> rows;
    bool anyChar = false, allChar = true;

    auto pushElem = [&](Value &&val, std::vector<Value> &dst) {
        if (val.isEmpty()) return;
        if (val.isChar()) anyChar = true;
        else              allChar = false;
        dst.push_back(std::move(val));
    };

    for (auto &rowNode : node->children) {
        std::vector<Value> rowElems;
        for (auto &elemNode : rowNode->children) {
            // CSL: `s.f` / `s.(name)` over a multi-element struct
            // array expands to one rowElem per element.
            if ((elemNode->type == NodeType::FIELD_ACCESS
                 || elemNode->type == NodeType::DYNAMIC_FIELD_ACCESS)
                && elemNode->children.size() >= 1) {
                auto base = execNode(elemNode->children[0].get(), env);
                if (base.isStruct() && base.isStructArray()) {
                    std::string fname;
                    if (elemNode->type == NodeType::FIELD_ACCESS) {
                        fname = elemNode->strValue;
                    } else {
                        // DYNAMIC_FIELD_ACCESS: child[1] is the name expr.
                        fname = execNode(elemNode->children[1].get(), env).toString();
                    }
                    for (size_t i = 0; i < base.numel(); ++i) {
                        const auto &m = base.structArrayElem(i);
                        auto it = m.find(fname);
                        if (it == m.end())
                            throw std::runtime_error(
                                "Reference to non-existent field '" + fname + "'");
                        pushElem(Value(it->second), rowElems);
                    }
                    continue;
                }
                // OBJECT array CSL: [arr.prop] expands prop over each
                // element via propGet. A scalar object falls through to
                // the generic path (keeps the property-or-method fallback).
                if (base.isObject() && base.objectCount() > 1) {
                    const BuiltinClass *cls = engine_.findClass(base.objectClassName());
                    std::string fname = (elemNode->type == NodeType::FIELD_ACCESS)
                        ? elemNode->strValue
                        : execNode(elemNode->children[1].get(), env).toString();
                    CallContext ctx{&engine_, env};
                    for (size_t i = 0; i < base.objectCount(); ++i) {
                        Value elem = base.objectSubArray({i}, engine_.mr_);
                        Value out;
                        if (!cls || !cls->propGet || !cls->propGet(elem, fname, out, ctx))
                            throw std::runtime_error("No appropriate property '" + fname
                                                     + "' for class '"
                                                     + base.objectClassName() + "'");
                        pushElem(std::move(out), rowElems);
                    }
                    continue;
                }
                // Single struct or non-struct base — fall through to the
                // generic execNode path so existing semantics apply.
            }
            auto val = execNode(elemNode.get(), env);
            pushElem(std::move(val), rowElems);
        }
        if (!rowElems.empty())
            rows.push_back(std::move(rowElems));
    }

    if (rows.empty())
        return Value();

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
            return Value::fromString(strs[0], engine_.mr_);

        // Build char matrix with space-padding
        size_t totalRows = strs.size();
        auto result = Value::matrix(totalRows, maxCols, ValueType::CHAR, engine_.mr_);
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
    std::vector<Value> rowValues;
    rowValues.reserve(rows.size());
    for (auto &rowElems : rows)
        rowValues.push_back(
            Value::horzcat(rowElems.data(), rowElems.size(), engine_.mr_));

    if (rowValues.size() == 1)
        return std::move(rowValues[0]);

    return Value::vertcat(rowValues.data(), rowValues.size(), engine_.mr_);
}

Value TreeWalker::execCellLiteral(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return Value::cell(0, 0);

    bool is2D = !node->children.empty() && node->children[0]->type == NodeType::BLOCK;

    if (!is2D) {
        auto cell = Value::cell(1, node->children.size());
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

    auto cell = Value::cell(numRows, numCols);

    for (size_t r = 0; r < numRows; ++r) {
        auto &rowNode = node->children[r];
        for (size_t c = 0; c < numCols; ++c) {
            cell.cellAt(cell.dims().sub2ind(r, c)) = execNode(rowNode->children[c].get(), env);
        }
    }

    return cell;
}

// ============================================================
// Pick the dominant non-double type among colon operands. MATLAB rule:
//   - If all operands are double → return DOUBLE.
//   - If exactly one non-double type T appears (others may be DOUBLE) → T.
//   - Two different non-double types → throw (matches MATLAB:
//     "Colon operands must be all the same type, or mixed with real
//     scalar doubles").
static ValueType colonOutputType(const Value *ops, size_t n)
{
    ValueType nonDouble = ValueType::DOUBLE;
    bool found = false;
    for (size_t i = 0; i < n; ++i) {
        ValueType t = ops[i].type();
        if (t == ValueType::DOUBLE) continue;
        if (!found) { nonDouble = t; found = true; }
        else if (t != nonDouble) {
            throw std::runtime_error(
                "Colon operands must be all the same type, "
                "or mixed with real scalar doubles");
        }
    }
    return nonDouble;
}

Value TreeWalker::execColonExpr(const ASTNode *node, Environment *env)
{
    if (node->children.empty())
        return Value::fromString(":", engine_.mr_);

    if (node->children.size() == 2) {
        Value a = execNode(node->children[0].get(), env);
        Value b = execNode(node->children[1].get(), env);
        Value ops[2] = {a, b};
        ValueType t = colonOutputType(ops, 2);
        return Value::colonRangeTyped(a.toScalar(), b.toScalar(), t, engine_.mr_);
    }

    if (node->children.size() == 3) {
        Value a = execNode(node->children[0].get(), env);
        Value b = execNode(node->children[1].get(), env);
        Value c = execNode(node->children[2].get(), env);
        Value ops[3] = {a, b, c};
        ValueType t = colonOutputType(ops, 3);
        return Value::colonRangeTyped(a.toScalar(), b.toScalar(), c.toScalar(),
                                       t, engine_.mr_);
    }

    return Value();
}

// ============================================================
Value TreeWalker::execIf(const ASTNode *node, Environment *env)
{
    for (auto &[cond, body] : node->branches) {
        Value condM;
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
    return Value();
}

Value TreeWalker::execFor(const ASTNode *node, Environment *env)
{
    const std::string &varName = node->strValue;
    auto rangeVal = execNode(node->children[0].get(), env);

    // Empty range — body never executes (MATLAB behavior)
    if (rangeVal.isEmpty())
        return Value();

    if (rangeVal.isCell()) {
        size_t cols = rangeVal.dims().cols();
        size_t rows = rangeVal.dims().rows();
        for (size_t c = 0; c < cols; ++c) {
            if (rows == 1) {
                env->set(varName, rangeVal.cellAt(c));
            } else {
                auto col = Value::cell(rows, 1);
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
                return Value();
        }
        return Value();
    }

    if (rangeVal.type() == ValueType::DOUBLE) {
        auto dims = rangeVal.dims();
        if (dims.rows() == 1) {
            // ── Fast path: scalar iteration over row vector ──
            // Set the variable once, then update its value in-place
            // to avoid env->set() hash lookup + Value creation each iteration.
            const double *src = rangeVal.doubleData();
            env->set(varName, Value::scalar(0.0, engine_.mr_));
            Value *varPtr = env->get(varName);
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
                    return Value();
                // Re-fetch pointer: body may have reassigned the loop variable
                varPtr = env->get(varName);
                if (!varPtr || !varPtr->isScalar() || varPtr->type() != ValueType::DOUBLE) {
                    // Variable was changed to non-scalar — fall back to slow path
                    for (size_t c2 = c + 1; c2 < dims.cols(); ++c2) {
                        env->set(varName, Value::scalar(src[c2], engine_.mr_));
                        execNode(node->children[1].get(), env);
                        if (flowSignal_ == FlowSignal::BREAK) {
                            flowSignal_ = FlowSignal::NONE;
                            return Value();
                        }
                        if (flowSignal_ == FlowSignal::CONTINUE) {
                            flowSignal_ = FlowSignal::NONE;
                            continue;
                        }
                        if (flowSignal_ == FlowSignal::RETURN)
                            return Value();
                    }
                    return Value();
                }
                slot = varPtr->doubleDataMut();
            }
        } else {
            for (size_t c = 0; c < dims.cols(); ++c) {
                auto col = Value::matrix(dims.rows(), 1, ValueType::DOUBLE, engine_.mr_);
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
                    return Value();
            }
        }
        return Value();
    }

    if (rangeVal.isChar()) {
        const char *cd = rangeVal.charData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, Value::fromString(std::string(1, cd[i]), engine_.mr_));
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
                return Value();
        }
        return Value();
    }

    if (rangeVal.isLogical()) {
        const uint8_t *ld = rangeVal.logicalData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, Value::scalar(static_cast<double>(ld[i]), engine_.mr_));
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
                return Value();
        }
        return Value();
    }

    throw std::runtime_error("Unsupported type in for loop: "
                             + std::string(mtypeName(rangeVal.type())));
}

Value TreeWalker::execWhile(const ASTNode *node, Environment *env)
{
    auto *condNode = node->children[0].get();
    for (;;) {
        Value condM;
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
            return Value();
    }
    return Value();
}

Value TreeWalker::execSwitch(const ASTNode *node, Environment *env)
{
    auto sv = execNode(node->children[0].get(), env);

    for (auto &[ce, body] : node->branches) {
        auto cv = execNode(ce.get(), env);
        bool matched = false;

        // isequal-based matching (MATLAB semantics)
        auto valuesEqual = [](const Value &a, const Value &b) -> bool {
            if (a.type() != b.type()) return false;
            if (a.isChar() && b.isChar()) return a.toString() == b.toString();
            if (a.numel() != b.numel()) return false;
            if (a.dims() != b.dims()) return false;
            if (a.isScalar() && b.isScalar()) return a.toScalar() == b.toScalar();
            if (a.type() == ValueType::DOUBLE) {
                const double *da = a.doubleData(), *db = b.doubleData();
                for (size_t i = 0; i < a.numel(); ++i)
                    if (da[i] != db[i]) return false;
                return true;
            }
            if (a.type() == ValueType::LOGICAL) {
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
    return Value();
}

// ============================================================
Value TreeWalker::execFunctionDef(const ASTNode *node, Environment *env)
{
    (void) env;
    UserFunction func;
    func.name = node->strValue;
    func.params = node->paramNames;
    func.returns = node->returnNames;
    func.body = std::shared_ptr<const ASTNode>(cloneNode(node->children[0].get()));
    func.closureEnv = nullptr;
    // Route to the script-lexical bucket while inside a script
    // scope — matches the compiler's rule so `clear all` in TW
    // mode behaves the same as in VM mode (local functions survive).
    auto *cp = engine_.compilerPtr();
    if (cp && cp->inScriptScope())
        engine_.scriptLocalUserFuncs_[func.name] = std::move(func);
    else
        engine_.userFuncs_[func.name] = std::move(func);
    return Value();
}

Value TreeWalker::execExprStmt(const ASTNode *node, Environment *env)
{
    auto *child = node->children[0].get();

    // MATLAB display / ans rule (see Compiler::compileExprStmt for the
    // VM mirror). Bare read of a user variable → display by its own name,
    // no ans. Everything else → ans store regardless of semicolon, "ans"
    // label for display.
    bool bareUserVar = false;
    std::string displayName;
    if (child->type == NodeType::IDENTIFIER) {
        const std::string &name = child->strValue;
        if (env->getLocal(name)) {
            bareUserVar = true;
            displayName = name;
        }
    }

    // Statement context: dispatch CALL and IDENTIFIER with nargout=0
    Value val;
    if (child->type == NodeType::CALL)
        val = execCall(child, env, 0);
    else if (child->type == NodeType::IDENTIFIER)
        val = execIdentifier(child, env, 0);
    else
        val = execNode(child, env);

    if (bareUserVar) {
        if (!node->suppressOutput)
            displayValue(displayName, val);
        return val;
    }

    // Anonymous value — bind to `ans` regardless of semicolon (matches
    // MATLAB; `ans` persists even when display is suppressed).
    if (!val.isEmpty()) {
        env->set("ans", val);
        if (!node->suppressOutput)
            displayValue("ans", val);
    }
    return val;
}

// ============================================================
Value TreeWalker::execCommandCall(const ASTNode *node, Environment *env)
{
    const std::string &name = node->strValue;

    std::vector<Value> args;
    args.reserve(node->children.size());
    for (auto &child : node->children)
        args.push_back(Value::fromString(child->strValue, engine_.mr_));

    // 1. User functions take precedence over external builtins
    //    (MATLAB semantics: a user-defined `function y = clear(x)` shadows
    //    the `clear` builtin even in command-style call form).
    Value result;
    if (auto *_uf = engine_.lookupUserFunction(name, env)) {
        result = callUserFunction(*_uf, args, env);
        if (!node->suppressOutput && !result.isEmpty()) {
            env->set("ans", result);
            displayValue("ans", result);
        }
        return result;
    }

    // 2. External registered functions (includes workspace builtins,
    //    import-aware via findExternal).
    if (const ExternalFunc *fn = engine_.findExternal(name, env)) {
        Value outBuf[1];
        CallContext ctx{&engine_, env};
        (*fn)(args, 0, Span<Value>(outBuf, 1), ctx);
        result = outBuf[0];
        if (!node->suppressOutput && !result.isEmpty()) {
            env->set("ans", result);
            displayValue("ans", result);
        }
        return result;
    }

    throw std::runtime_error("Undefined function: " + name);
}

// ============================================================
Value TreeWalker::execAnonFunc(const ASTNode *node, Environment *env)
{
    if (!node->strValue.empty() && node->children.empty())
        return Value::funcHandle(node->strValue, engine_.mr_);

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
                                  engine_.globalsEnv_.get());

    engine_.userFuncs_[anonName] = std::move(uf);
    return Value::funcHandle(anonName, engine_.mr_);
}

// ============================================================
Value TreeWalker::execTryCatch(const ASTNode *node, Environment *env)
{
    try {
        auto result = execNode(node->children[0].get(), env);
        // Propagate flow signals — try/catch doesn't intercept break/continue/return
        if (flowSignal_ != FlowSignal::NONE)
            return result;
        return result;
    } catch (const Error &mle) {
        if (node->children.size() > 1) {
            if (!node->strValue.empty()) {
                auto err = Value::structure();
                err.field("message") = Value::fromString(mle.what(), engine_.mr_);
                std::string id = mle.identifier().empty() ? "numkit:error" : mle.identifier();
                err.field("identifier") = Value::fromString(id, engine_.mr_);
                env->set(node->strValue, err);
            }
            return execNode(node->children[1].get(), env);
        }
        return Value();
    } catch (const std::exception &e) {
        if (node->children.size() > 1) {
            if (!node->strValue.empty()) {
                auto err = Value::structure();
                err.field("message") = Value::fromString(e.what(), engine_.mr_);
                err.field("identifier") = Value::fromString("numkit:error", engine_.mr_);
                env->set(node->strValue, err);
            }
            return execNode(node->children[1].get(), env);
        }
        return Value();
    }
}

// ============================================================
Value TreeWalker::execGlobalPersistent(const ASTNode *node, Environment *env)
{
    for (auto &name : node->paramNames) {
        env->declareGlobal(name);
        if (!engine_.globalsEnv_->get(name))
            engine_.globalsEnv_->set(name, Value());
    }
    return Value();
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

// Extract bare-identifier arg names from a CALL AST node for inputname(k).
// Returns empty vector if callNode is null. Reserved names (pi, eps, …)
// are treated as non-identifier (returned as empty string).
static std::vector<std::string> extractCallerArgNames(const ASTNode *callNode,
                                                       const Engine &engine)
{
    std::vector<std::string> names;
    if (!callNode || callNode->children.size() < 2) return names;
    names.reserve(callNode->children.size() - 1);
    bool anyIdent = false;
    for (size_t i = 1; i < callNode->children.size(); ++i) {
        const auto &child = callNode->children[i];
        if (child && child->type == NodeType::IDENTIFIER
            && !engine.isReservedName(child->strValue)) {
            names.push_back(child->strValue);
            anyIdent = true;
        } else {
            names.emplace_back();
        }
    }
    if (!anyIdent) names.clear();
    return names;
}

Value TreeWalker::callUserFunction(const UserFunction &func,
                                    Span<const Value> args,
                                    Environment *env,
                                    const ASTNode *callNode)
{
    RecursionGuard rguard(currentRecursionDepth_, maxRecursionDepth_);

    if (args.size() > func.params.size())
        throw std::runtime_error("Too many input arguments for function '" + func.name + "'");

    // Regular functions: parent = constantsEnv (see pi/eps/inf but NOT global variables)
    // Closures: parent = captured scope (already contains correct chain)
    Environment *parentEnv = func.closureEnv ? func.closureEnv.get()
                                             : &engine_.constantsEnv();
    Environment localEnv(parentEnv, engine_.globalsEnv_.get());
    FrameGuard frameGuard(activeFrames_, &localEnv,
                          extractCallerArgNames(callNode, engine_));

    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i)
        localEnv.setLocal(func.params[i], args[i]);

    // Always set nargin/nargout — VM unconditionally reserves slots for
    // them (compiler.cpp:2986-2987), and TW's old astUsesIdentifier
    // optimization missed references hidden inside eval'd string
    // literals (`eval('nargin')`), causing TW/VM divergence.
    func.usesNarginNargout = 1;
    if (func.usesNarginNargout) {
        size_t nout = std::max(func.returns.size(), size_t(1));
        localEnv.setLocal("nargin",
                          Value::scalar(static_cast<double>(args.size()), engine_.mr_));
        localEnv.setLocal("nargout", Value::scalar(static_cast<double>(nout), engine_.mr_));
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
        return Value();

    auto *val = localEnv.getLocal(func.returns[0]);
    if (!val)
        val = localEnv.get(func.returns[0]);
    return val ? std::move(*val) : Value();
}

std::vector<Value> TreeWalker::callUserFunctionMulti(const UserFunction &func,
                                                      Span<const Value> args,
                                                      Environment *env,
                                                      size_t nout,
                                                      const ASTNode *callNode)
{
    RecursionGuard rguard(currentRecursionDepth_, maxRecursionDepth_);

    if (args.size() > func.params.size())
        throw std::runtime_error("Too many input arguments for function '" + func.name + "'");

    Environment *parentEnv = func.closureEnv ? func.closureEnv.get()
                                             : &engine_.constantsEnv();
    Environment localEnv(parentEnv, engine_.globalsEnv_.get());
    FrameGuard frameGuard(activeFrames_, &localEnv,
                          extractCallerArgNames(callNode, engine_));

    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i)
        localEnv.setLocal(func.params[i], args[i]);

    if (func.usesNarginNargout == -1)
        func.usesNarginNargout = astUsesIdentifier(func.body.get(), "nargin", "nargout") ? 1 : 0;
    if (func.usesNarginNargout) {
        localEnv.setLocal("nargin",
                          Value::scalar(static_cast<double>(args.size()), engine_.mr_));
        localEnv.setLocal("nargout", Value::scalar(static_cast<double>(nout), engine_.mr_));
    }

    for (auto &retName : func.returns)
        if (!localEnv.getLocal(retName))
            localEnv.setLocal(retName, Value());

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

    std::vector<Value> results;
    results.reserve(std::min(func.returns.size(), nout));
    for (size_t i = 0; i < func.returns.size() && i < nout; ++i) {
        auto *val = localEnv.getLocal(func.returns[i]);
        if (!val)
            val = localEnv.get(func.returns[i]);
        results.push_back(val ? std::move(*val) : Value());
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

} // namespace numkit