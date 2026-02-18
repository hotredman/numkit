// src/MLabEngine.cpp
#include "MLabEngine.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace mlab {

// ============================================================
// Construction & registration
// ============================================================
Engine::Engine()
{
    allocator_ = Allocator::defaultAllocator();
    globalEnv_ = std::make_shared<Environment>(nullptr, &globalStore_);

    // Standard MATLAB constants
    globalEnv_->set("pi", MValue::scalar(3.14159265358979323846, &allocator_));
    globalEnv_->set("eps", MValue::scalar(2.2204460492503131e-16, &allocator_));
    globalEnv_->set("inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    globalEnv_->set("Inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    globalEnv_->set("nan", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    globalEnv_->set("NaN", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    globalEnv_->set("true", MValue::logicalScalar(true, &allocator_));
    globalEnv_->set("false", MValue::logicalScalar(false, &allocator_));
    globalEnv_->set("i", MValue::complexScalar(0.0, 1.0, &allocator_)); // ← заменить
    globalEnv_->set("j", MValue::complexScalar(0.0, 1.0, &allocator_)); // ← заменить
}

void Engine::setAllocator(Allocator alloc)
{
    allocator_ = std::move(alloc);
}
Allocator &Engine::allocator()
{
    return allocator_;
}

void Engine::registerBinaryOp(const std::string &op, BinaryOpFunc func)
{
    binaryOps_[op] = std::move(func);
}

void Engine::registerUnaryOp(const std::string &op, UnaryOpFunc func)
{
    unaryOps_[op] = std::move(func);
}

void Engine::registerFunction(const std::string &name, ExternalFunc func)
{
    externalFuncs_[name] = std::move(func);
}

void Engine::setVariable(const std::string &name, MValue val)
{
    globalEnv_->set(name, std::move(val));
}

MValue *Engine::getVariable(const std::string &name)
{
    return globalEnv_->get(name);
}

void Engine::setOutputFunc(OutputFunc f)
{
    outputFunc_ = std::move(f);
}
void Engine::setMaxRecursionDepth(int d)
{
    maxRecursionDepth_ = d;
}

bool Engine::isKnownFunction(const std::string &name) const
{
    return externalFuncs_.count(name) || userFuncs_.count(name);
}

// ============================================================
// Output helpers
// ============================================================
void Engine::output(const std::string &s)
{
    if (outputFunc_)
        outputFunc_(s);
    else
        std::cout << s;
}

void Engine::displayValue(const std::string &name, const MValue &val)
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
    case MType::EMPTY:
        os << "    []\n";
        break;
    default:
        os << "    " << val.debugString() << "\n";
        break;
    }
    output(os.str());
}

// ============================================================
// eval
// ============================================================
MValue Engine::eval(const std::string &code)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    return execNode(ast.get(), globalEnv_);
}

// ============================================================
// Colon helper
// ============================================================
double Engine::colonCount(double start, double step, double stop) const
{
    if (step == 0.0)
        throw std::runtime_error("Colon step cannot be zero");
    if ((step > 0 && stop < start) || (step < 0 && stop > start))
        return 0;
    double n = std::floor((stop - start) / step + 0.5) + 1;
    if (n < 0)
        n = 0;
    double last = start + (n - 1) * step;
    if (step > 0 && last > stop + 0.5 * std::abs(step))
        n--;
    if (step < 0 && last < stop - 0.5 * std::abs(step))
        n--;
    if (n < 0)
        n = 0;
    return n;
}

// ============================================================
// resolveIndex
// ============================================================
std::vector<size_t> Engine::resolveIndex(const ASTNode *indexExpr,
                                         const MValue &array,
                                         int dim,
                                         int ndims,
                                         std::shared_ptr<Environment> env)
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
// execNode — main dispatch
// ============================================================
MValue Engine::execNode(const ASTNode *node, std::shared_ptr<Environment> env)
{
    if (!node)
        return MValue::empty();

    switch (node->type) {
    case NodeType::BLOCK:
        return execBlock(node, env);
    case NodeType::NUMBER_LITERAL:
        return MValue::scalar(node->numValue, &allocator_);
    case NodeType::STRING_LITERAL:
        return MValue::fromString(node->strValue, &allocator_);
    case NodeType::BOOL_LITERAL:
        return MValue::logicalScalar(node->boolValue, &allocator_);
    case NodeType::IMAG_LITERAL:                                        // ← добавить
        return MValue::complexScalar(0.0, node->numValue, &allocator_); // ← добавить
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
        throw BreakSignal{};
    case NodeType::CONTINUE_STMT:
        throw ContinueSignal{};
    case NodeType::RETURN_STMT:
        throw ReturnSignal{};
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
    case NodeType::END_VAL: {
        if (!indexContextStack_.empty()) {
            auto &ctx = indexContextStack_.back();
            size_t sz = (ctx.ndims == 1) ? ctx.array->numel()
                                         : ctx.array->dims().dimSize(ctx.dimension);
            return MValue::scalar(static_cast<double>(sz), &allocator_);
        }
        throw std::runtime_error("'end' used outside of indexing context");
    }
    default:
        throw std::runtime_error("Unknown AST node type");
    }
}

MValue Engine::execBlock(const ASTNode *node, std::shared_ptr<Environment> env)
{
    MValue last = MValue::empty();
    for (auto &child : node->children)
        last = execNode(child.get(), env);
    return last;
}

// ============================================================
// Identifier
// ============================================================
MValue Engine::execIdentifier(const ASTNode *node, std::shared_ptr<Environment> env)
{
    const std::string &name = node->strValue;

    auto *val = env->get(name);
    if (val)
        return *val;

    // Try built-in with zero args
    MValue result;
    if (tryBuiltinCall(name, {}, env, result))
        return result;

    if (externalFuncs_.count(name)) {
        auto res = externalFuncs_[name]({});
        return res.empty() ? MValue::empty() : res[0];
    }
    if (userFuncs_.count(name))
        return callUserFunction(userFuncs_[name], {}, env);

    throw std::runtime_error("Undefined variable or function: " + name);
}
// ============================================================
// Assignment
// ============================================================
MValue Engine::execAssign(const ASTNode *node, std::shared_ptr<Environment> env)
{
    auto *lhs = node->children[0].get();
    auto rhs = execNode(node->children[1].get(), env);

    if (lhs->type == NodeType::IDENTIFIER) {
        env->set(lhs->strValue, rhs);
        if (!node->suppressOutput)
            displayValue(lhs->strValue, rhs);
    } else if (lhs->type == NodeType::CALL) {
        execIndexedAssign(lhs, rhs, env);
    } else if (lhs->type == NodeType::FIELD_ACCESS) {
        execFieldAssign(lhs, rhs, env);
    } else if (lhs->type == NodeType::CELL_INDEX) {
        execCellAssign(lhs, rhs, env);
    } else {
        throw std::runtime_error("Invalid assignment target");
    }
    return rhs;
}

void Engine::execIndexedAssign(const ASTNode *lhs,
                               const MValue &rhs,
                               std::shared_ptr<Environment> env)
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
        auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
        for (auto idx : indices)
            var->ensureSize(idx, &allocator_);
        if (rhs.isScalar()) {
            double sv = rhs.toScalar();
            double *data = var->doubleDataMut();
            for (auto idx : indices)
                data[idx] = sv;
        } else if (rhs.type() == MType::DOUBLE) {
            const double *src = rhs.doubleData();
            double *dst = var->doubleDataMut();
            for (size_t i = 0; i < indices.size() && i < rhs.numel(); ++i)
                dst[indices[i]] = src[i];
        }
    } else if (nargs == 2) {
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
                        &allocator_);

        double *dst = var->doubleDataMut();
        auto &dims = var->dims();
        if (rhs.isScalar()) {
            double sv = rhs.toScalar();
            for (auto c : colIdx)
                for (auto r : rowIdx)
                    dst[dims.sub2ind(r, c)] = sv;
        } else if (rhs.type() == MType::DOUBLE) {
            const double *src = rhs.doubleData();
            size_t k = 0;
            for (size_t ci = 0; ci < colIdx.size(); ++ci)
                for (size_t ri = 0; ri < rowIdx.size(); ++ri)
                    if (k < rhs.numel())
                        dst[dims.sub2ind(rowIdx[ri], colIdx[ci])] = src[k++];
        }
    }
}

MValue &Engine::resolveFieldLValue(const ASTNode *node, std::shared_ptr<Environment> env)
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

void Engine::execFieldAssign(const ASTNode *lhs, const MValue &rhs, std::shared_ptr<Environment> env)
{
    resolveFieldLValue(lhs, env) = rhs;
}

void Engine::execCellAssign(const ASTNode *lhs, const MValue &rhs, std::shared_ptr<Environment> env)
{
    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Invalid cell assignment target");

    auto *var = env->get(target->strValue);
    if (!var)
        throw std::runtime_error("Undefined variable: " + target->strValue);
    if (!var->isCell())
        throw std::runtime_error("Cell indexing on non-cell variable: " + target->strValue);

    IndexContextGuard guard(indexContextStack_, {var, 0, 1});
    MValue idx = execNode(lhs->children[1].get(), env);

    var->cellAt(static_cast<size_t>(idx.toScalar()) - 1) = rhs;
}

// ============================================================
// Multi-assignment
// ============================================================
MValue Engine::execMultiAssign(const ASTNode *node, std::shared_ptr<Environment> env)
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

std::vector<MValue> Engine::execCallMulti(const ASTNode *node,
                                          std::shared_ptr<Environment> env,
                                          size_t nout)
{
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

    if (externalFuncs_.count(funcName))
        return externalFuncs_[funcName](args);
    if (userFuncs_.count(funcName))
        return callUserFunctionMulti(userFuncs_[funcName], args, env, nout);

    throw std::runtime_error("Undefined function: " + funcName);
}

// ============================================================
// Delete: A(idx) = []
// ============================================================
MValue Engine::execDeleteAssign(const ASTNode *node, std::shared_ptr<Environment> env)
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

    if (nargs == 1 && var->type() == MType::DOUBLE) {
        auto indices = resolveIndex(lhs->children[1].get(), *var, 0, 1, env);
        std::vector<bool> del(var->numel(), false);
        for (auto idx : indices) {
            if (idx < var->numel())
                del[idx] = true;
        }
        const double *src = var->doubleData();
        std::vector<double> remaining;
        remaining.reserve(var->numel());
        for (size_t i = 0; i < var->numel(); ++i)
            if (!del[i])
                remaining.push_back(src[i]);

        bool isRow = var->dims().rows() == 1;
        size_t n = remaining.size();
        auto result = isRow ? MValue::matrix(1, n, MType::DOUBLE, &allocator_)
                            : MValue::matrix(n, 1, MType::DOUBLE, &allocator_);
        if (n > 0) {
            double *dst = result.doubleDataMut();
            std::memcpy(dst, remaining.data(), n * sizeof(double));
        }
        *var = result;
    } else if (nargs == 2 && var->type() == MType::DOUBLE) {
        auto rowIdx = resolveIndex(lhs->children[1].get(), *var, 0, 2, env);
        auto colIdx = resolveIndex(lhs->children[2].get(), *var, 1, 2, env);
        size_t R = var->dims().rows(), C = var->dims().cols();

        if (colIdx.size() == C) {
            std::vector<bool> delR(R, false);
            for (auto r : rowIdx)
                if (r < R)
                    delR[r] = true;
            size_t newR = std::count(delR.begin(), delR.end(), false);
            auto result = MValue::matrix(newR, C, MType::DOUBLE, &allocator_);
            double *dst = result.doubleDataMut();
            size_t ri = 0;
            for (size_t r = 0; r < R; ++r) {
                if (!delR[r]) {
                    for (size_t c = 0; c < C; ++c)
                        dst[c * newR + ri] = (*var)(r, c);
                    ri++;
                }
            }
            *var = result;
        } else if (rowIdx.size() == R) {
            std::vector<bool> delC(C, false);
            for (auto c : colIdx)
                if (c < C)
                    delC[c] = true;
            size_t newC = std::count(delC.begin(), delC.end(), false);
            auto result = MValue::matrix(R, newC, MType::DOUBLE, &allocator_);
            double *dst = result.doubleDataMut();
            size_t ci = 0;
            for (size_t c = 0; c < C; ++c) {
                if (!delC[c]) {
                    for (size_t r = 0; r < R; ++r)
                        dst[ci * R + r] = (*var)(r, c);
                    ci++;
                }
            }
            *var = result;
        } else {
            throw std::runtime_error("Deletion requires full row or column specification");
        }
    }
    return MValue::empty();
}

// ============================================================
// Binary & unary operators
// ============================================================
MValue Engine::execBinaryOp(const ASTNode *node, std::shared_ptr<Environment> env)
{
    const std::string &op = node->strValue;

    if (op == "&&") {
        auto l = execNode(node->children[0].get(), env);
        if (!l.toBool())
            return MValue::logicalScalar(false, &allocator_);
        return MValue::logicalScalar(execNode(node->children[1].get(), env).toBool(), &allocator_);
    }
    if (op == "||") {
        auto l = execNode(node->children[0].get(), env);
        if (l.toBool())
            return MValue::logicalScalar(true, &allocator_);
        return MValue::logicalScalar(execNode(node->children[1].get(), env).toBool(), &allocator_);
    }

    auto left = execNode(node->children[0].get(), env);
    auto right = execNode(node->children[1].get(), env);

    auto it = binaryOps_.find(op);
    if (it != binaryOps_.end())
        return it->second(left, right);

    throw std::runtime_error("Undefined binary operator: " + op);
}

MValue Engine::execUnaryOp(const ASTNode *node, std::shared_ptr<Environment> env)
{
    auto operand = execNode(node->children[0].get(), env);
    auto it = unaryOps_.find(node->strValue);
    if (it != unaryOps_.end())
        return it->second(operand);
    throw std::runtime_error("Undefined unary operator: " + node->strValue);
}

// ============================================================
// Function call / indexing
// ============================================================
MValue Engine::callFuncHandle(const MValue &handle,
                              const std::vector<MValue> &args,
                              std::shared_ptr<Environment> env)
{
    auto results = callFuncHandleMulti(handle, args, env, 1);
    return results.empty() ? MValue::empty() : results[0];
}

std::vector<MValue> Engine::callFuncHandleMulti(const MValue &handle,
                                                const std::vector<MValue> &args,
                                                std::shared_ptr<Environment> env,
                                                size_t nout)
{
    const std::string &name = handle.funcHandleName();
    if (externalFuncs_.count(name))
        return externalFuncs_[name](args);
    if (userFuncs_.count(name))
        return callUserFunctionMulti(userFuncs_[name], args, env, nout);
    throw std::runtime_error("Undefined function in handle: @" + name);
}

MValue Engine::execCall(const ASTNode *node, std::shared_ptr<Environment> env)
{
    auto *funcNode = node->children[0].get();

    // FIX #1: Поддержка chain calls — target может быть любым выражением
    // Если target — не идентификатор, вычисляем его и пытаемся вызвать/проиндексировать
    if (funcNode->type != NodeType::IDENTIFIER) {
        auto target = execNode(funcNode, env);

        if (target.isFuncHandle()) {
            std::vector<MValue> args;
            args.reserve(node->children.size() - 1);
            for (size_t i = 1; i < node->children.size(); ++i)
                args.push_back(execNode(node->children[i].get(), env));
            return callFuncHandle(target, args, env);
        }

        // Если результат — массив/cell/etc., трактуем как индексацию
        if (target.isNumeric() || target.isLogical() || target.isChar() || target.isCell())
            return execIndexAccess(target, node, env);

        throw std::runtime_error("Cannot call or index into value of type "
                                 + std::string(mtypeName(target.type())));
    }

    const std::string &name = funcNode->strValue;

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

    // Try built-in commands (need env access)
    {
        auto args = buildArgs();
        MValue result;
        if (tryBuiltinCall(name, args, env, result))
            return result;
    }

    if (externalFuncs_.count(name)) {
        auto args = buildArgs();
        auto res = externalFuncs_[name](args);
        return res.empty() ? MValue::empty() : res[0];
    }
    if (userFuncs_.count(name)) {
        auto args = buildArgs();
        return callUserFunction(userFuncs_[name], args, env);
    }

    if (var) {
        throw std::runtime_error("Cannot index into variable '" + name + "' of type "
                                 + std::string(mtypeName(var->type())) + ", and no function '"
                                 + name + "' was found");
    }
    throw std::runtime_error("Undefined function or variable: " + name);
}

MValue Engine::execIndexAccess(const MValue &var,
                               const ASTNode *callNode,
                               std::shared_ptr<Environment> env)
{
    size_t nargs = callNode->children.size() - 1;

    // FIX #7: Bounds check helper for read-access
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
            return MValue::fromString(result, &allocator_);
        }
        throw std::runtime_error("Multi-dimensional char indexing not supported");
    }

    if (var.isCell()) {
        if (nargs == 1) {
            auto indices = resolveIndex(callNode->children[1].get(), var, 0, 1, env);
            if (indices.size() == 1)
                return var.cellAt(indices[0]);
            auto result = MValue::cell(1, indices.size());
            for (size_t i = 0; i < indices.size(); ++i)
                result.cellAt(i) = var.cellAt(indices[i]);
            return result;
        }
        if (nargs == 2) {
            auto ri = resolveIndex(callNode->children[1].get(), var, 0, 2, env);
            auto ci = resolveIndex(callNode->children[2].get(), var, 1, 2, env);
            if (ri.size() == 1 && ci.size() == 1)
                return var.cellAt(var.dims().sub2ind(ri[0], ci[0]));
            auto result = MValue::cell(ri.size(), ci.size());
            for (size_t c = 0; c < ci.size(); ++c)
                for (size_t r = 0; r < ri.size(); ++r)
                    result.cellAt(r + c * ri.size()) = var.cellAt(var.dims().sub2ind(ri[r], ci[c]));
            return result;
        }
        throw std::runtime_error("Cell indexing with more than 2 dimensions not supported");
    }

    if (nargs == 1) {
        auto indices = resolveIndex(callNode->children[1].get(), var, 0, 1, env);
        checkBounds(indices, var.numel(), "linear index");
        if (var.isLogical()) {
            const uint8_t *ld = var.logicalData();
            if (indices.size() == 1)
                return MValue::logicalScalar(ld[indices[0]] != 0, &allocator_);
            auto result = MValue::matrix(1, indices.size(), MType::LOGICAL, &allocator_);
            uint8_t *dst = result.logicalDataMut();
            for (size_t i = 0; i < indices.size(); ++i)
                dst[i] = ld[indices[i]];
            return result;
        }
        const double *dd = var.doubleData();
        if (indices.size() == 1)
            return MValue::scalar(dd[indices[0]], &allocator_);
        auto result = MValue::matrix(1, indices.size(), MType::DOUBLE, &allocator_);
        double *dst = result.doubleDataMut();
        for (size_t i = 0; i < indices.size(); ++i)
            dst[i] = dd[indices[i]];
        return result;
    }
    if (nargs == 2) {
        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 2, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 2, env);
        checkBounds(ri, var.dims().rows(), "row index");
        checkBounds(ci, var.dims().cols(), "column index");
        if (ri.size() == 1 && ci.size() == 1)
            return MValue::scalar(var(ri[0], ci[0]), &allocator_);
        auto result = MValue::matrix(ri.size(), ci.size(), MType::DOUBLE, &allocator_);
        double *dst = result.doubleDataMut();
        for (size_t c = 0; c < ci.size(); ++c)
            for (size_t r = 0; r < ri.size(); ++r)
                dst[c * ri.size() + r] = var(ri[r], ci[c]);
        return result;
    }
    if (nargs == 3) {
        auto ri = resolveIndex(callNode->children[1].get(), var, 0, 3, env);
        auto ci = resolveIndex(callNode->children[2].get(), var, 1, 3, env);
        auto pi = resolveIndex(callNode->children[3].get(), var, 2, 3, env);
        if (ri.size() == 1 && ci.size() == 1 && pi.size() == 1)
            return MValue::scalar(var(ri[0], ci[0], pi[0]), &allocator_);
        auto result = MValue::matrix3d(ri.size(), ci.size(), pi.size(), MType::DOUBLE, &allocator_);
        double *dst = result.doubleDataMut();
        Dims rd(ri.size(), ci.size(), pi.size());
        for (size_t p = 0; p < pi.size(); ++p)
            for (size_t c = 0; c < ci.size(); ++c)
                for (size_t r = 0; r < ri.size(); ++r)
                    dst[rd.sub2ind(r, c, p)] = var(ri[r], ci[c], pi[p]);
        return result;
    }
    throw std::runtime_error("Too many indexing dimensions (max 3)");
}

// ============================================================
// Cell {}-indexing
// ============================================================
MValue Engine::execCellIndex(const ASTNode *node, std::shared_ptr<Environment> env)
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
    throw std::runtime_error("Cell indexing with more than 2 dimensions not supported");
}

MValue Engine::execFieldAccess(const ASTNode *node, std::shared_ptr<Environment> env)
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
// Matrix literal
// ============================================================
MValue Engine::execMatrixLiteral(const ASTNode *node, std::shared_ptr<Environment> env)
{
    if (node->children.empty())
        return MValue::empty();

    struct ElemInfo
    {
        MValue val;
        size_t rows = 1, cols = 1;
    };
    struct RowInfo
    {
        std::vector<ElemInfo> elems;
        size_t totalCols = 0, rowHeight = 1;
        bool allChar = true;
    };

    std::vector<RowInfo> matRows;
    bool anyChar = false;

    for (auto &rowNode : node->children) {
        RowInfo ri;
        for (auto &elemNode : rowNode->children) {
            auto val = execNode(elemNode.get(), env);

            // Skip empty values — MATLAB concatenation ignores []
            if (val.isEmpty())
                continue;

            size_t eR = 1, eC = 1;

            if (val.isChar()) {
                eC = val.numel();
                anyChar = true;
            } else if (val.type() == MType::DOUBLE) {
                eR = val.dims().rows();
                eC = val.dims().cols();
                ri.allChar = false;
            } else if (val.isLogical() && val.isScalar()) {
                val = MValue::scalar(val.toBool() ? 1.0 : 0.0, &allocator_);
                ri.allChar = false;
            } else {
                ri.allChar = false;
            }

            ri.totalCols += eC;
            if (eR > ri.rowHeight)
                ri.rowHeight = eR;
            ri.elems.push_back({std::move(val), eR, eC});
        }
        // Skip entirely empty rows
        if (!ri.elems.empty())
            matRows.push_back(std::move(ri));
    }

    // All elements were empty
    if (matRows.empty())
        return MValue::empty();

    bool allChar = true;
    for (auto &ri : matRows)
        if (!ri.allChar) {
            allChar = false;
            break;
        }

    if (allChar && anyChar) {
        if (matRows.size() == 1) {
            std::string result;
            for (auto &el : matRows[0].elems) {
                if (el.val.isChar())
                    result += el.val.toString();
            }
            return MValue::fromString(result, &allocator_);
        }
        size_t maxCols = 0;
        for (auto &ri : matRows)
            maxCols = std::max(maxCols, ri.totalCols);

        size_t totalRows = matRows.size();
        auto result = MValue::matrix(totalRows, maxCols, MType::CHAR, &allocator_);
        char *dst = result.charDataMut();
        std::memset(dst, ' ', totalRows * maxCols);

        for (size_t row = 0; row < matRows.size(); ++row) {
            size_t col = 0;
            for (auto &el : matRows[row].elems) {
                if (el.val.isChar()) {
                    const char *src = el.val.charData();
                    for (size_t i = 0; i < el.cols; ++i)
                        dst[i * totalRows + row] = src[i];
                    col += el.cols;
                }
            }
        }
        return result;
    }

    size_t totalRows = 0;
    size_t totalCols = matRows[0].totalCols;
    for (auto &ri : matRows) {
        totalRows += ri.rowHeight;
        if (ri.totalCols != totalCols)
            throw std::runtime_error("Dimensions of arrays being concatenated are not consistent");
    }

    auto result = MValue::matrix(totalRows, totalCols, MType::DOUBLE, &allocator_);
    double *dst = result.doubleDataMut();

    size_t rowOff = 0;
    for (auto &ri : matRows) {
        size_t colOff = 0;
        for (auto &el : ri.elems) {
            if (el.val.type() == MType::DOUBLE) {
                const double *src = el.val.doubleData();
                for (size_t c = 0; c < el.cols; ++c)
                    for (size_t r = 0; r < el.rows; ++r)
                        dst[(colOff + c) * totalRows + (rowOff + r)] = el.val.isScalar()
                                                                           ? src[0]
                                                                           : src[c * el.rows + r];
            } else if (el.val.isScalar()) {
                dst[colOff * totalRows + rowOff] = el.val.toScalar();
            } else if (el.val.isChar()) {
                const char *src = el.val.charData();
                for (size_t c = 0; c < el.cols; ++c)
                    dst[(colOff + c) * totalRows + rowOff] = static_cast<double>(
                        static_cast<unsigned char>(src[c]));
            }
            colOff += el.cols;
        }
        rowOff += ri.rowHeight;
    }
    return result;
}

MValue Engine::execCellLiteral(const ASTNode *node, std::shared_ptr<Environment> env)
{
    if (node->children.empty())
        return MValue::cell(0, 0);

    // Проверяем: если children содержат BLOCK-ноды — это 2D cell {a b; c d}
    // Если children содержат выражения напрямую — это старый формат (не должен
    // возникать с новым парсером, но для безопасности)
    bool is2D = !node->children.empty() && node->children[0]->type == NodeType::BLOCK;

    if (!is2D) {
        // Fallback: flat list — 1×N cell
        auto cell = MValue::cell(1, node->children.size());
        for (size_t i = 0; i < node->children.size(); ++i)
            cell.cellAt(i) = execNode(node->children[i].get(), env);
        return cell;
    }

    // 2D cell: каждый child — строка (BLOCK), внутри — элементы
    size_t numRows = node->children.size();
    size_t numCols = 0;

    // Определяем количество столбцов (по первой строке)
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
// Colon expression
// ============================================================
MValue Engine::execColonExpr(const ASTNode *node, std::shared_ptr<Environment> env)
{
    if (node->children.empty())
        return MValue::fromString(":", &allocator_);

    if (node->children.size() == 2) {
        double s = execNode(node->children[0].get(), env).toScalar();
        double e = execNode(node->children[1].get(), env).toScalar();
        size_t count = static_cast<size_t>(colonCount(s, 1.0, e));
        auto result = MValue::matrix(1, count, MType::DOUBLE, &allocator_);
        if (count > 0) {
            double *dst = result.doubleDataMut();
            for (size_t i = 0; i < count; ++i)
                dst[i] = s + static_cast<double>(i);
        }
        return result;
    }

    if (node->children.size() == 3) {
        double s = execNode(node->children[0].get(), env).toScalar();
        double step = execNode(node->children[1].get(), env).toScalar();
        double e = execNode(node->children[2].get(), env).toScalar();
        size_t count = static_cast<size_t>(colonCount(s, step, e));
        auto result = MValue::matrix(1, count, MType::DOUBLE, &allocator_);
        if (count > 0) {
            double *dst = result.doubleDataMut();
            for (size_t i = 0; i < count; ++i)
                dst[i] = s + static_cast<double>(i) * step;
            if (count >= 2) {
                double last = s + static_cast<double>(count - 1) * step;
                if (step > 0 && last > e)
                    dst[count - 1] = e;
                if (step < 0 && last < e)
                    dst[count - 1] = e;
            }
        }
        return result;
    }

    return MValue::empty();
}

// ============================================================
// Control flow
// ============================================================
MValue Engine::execIf(const ASTNode *node, std::shared_ptr<Environment> env)
{
    for (auto &[cond, body] : node->branches)
        if (execNode(cond.get(), env).toBool())
            return execNode(body.get(), env);
    if (node->elseBranch)
        return execNode(node->elseBranch.get(), env);
    return MValue::empty();
}

MValue Engine::execFor(const ASTNode *node, std::shared_ptr<Environment> env)
{
    const std::string &varName = node->strValue;
    auto rangeVal = execNode(node->children[0].get(), env);

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
            try {
                execNode(node->children[1].get(), env);
            } catch (BreakSignal &) {
                break;
            } catch (ContinueSignal &) {
                continue;
            }
        }
        return MValue::empty();
    }

    if (rangeVal.type() == MType::DOUBLE) {
        auto dims = rangeVal.dims();
        for (size_t c = 0; c < dims.cols(); ++c) {
            if (dims.rows() == 1) {
                env->set(varName, MValue::scalar(rangeVal(0, c), &allocator_));
            } else {
                auto col = MValue::matrix(dims.rows(), 1, MType::DOUBLE, &allocator_);
                double *dst = col.doubleDataMut();
                for (size_t r = 0; r < dims.rows(); ++r)
                    dst[r] = rangeVal(r, c);
                env->set(varName, col);
            }
            try {
                execNode(node->children[1].get(), env);
            } catch (BreakSignal &) {
                break;
            } catch (ContinueSignal &) {
                continue;
            }
        }
        return MValue::empty();
    }

    if (rangeVal.isChar()) {
        const char *cd = rangeVal.charData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, MValue::fromString(std::string(1, cd[i]), &allocator_));
            try {
                execNode(node->children[1].get(), env);
            } catch (BreakSignal &) {
                break;
            } catch (ContinueSignal &) {
                continue;
            }
        }
        return MValue::empty();
    }

    if (rangeVal.isLogical()) {
        const uint8_t *ld = rangeVal.logicalData();
        for (size_t i = 0; i < rangeVal.numel(); ++i) {
            env->set(varName, MValue::scalar(static_cast<double>(ld[i]), &allocator_));
            try {
                execNode(node->children[1].get(), env);
            } catch (BreakSignal &) {
                break;
            } catch (ContinueSignal &) {
                continue;
            }
        }
        return MValue::empty();
    }

    throw std::runtime_error("Unsupported type in for loop: "
                             + std::string(mtypeName(rangeVal.type())));
}

MValue Engine::execWhile(const ASTNode *node, std::shared_ptr<Environment> env)
{
    while (execNode(node->children[0].get(), env).toBool()) {
        try {
            execNode(node->children[1].get(), env);
        } catch (BreakSignal &) {
            break;
        } catch (ContinueSignal &) {
            continue;
        }
    }
    return MValue::empty();
}

MValue Engine::execSwitch(const ASTNode *node, std::shared_ptr<Environment> env)
{
    auto sv = execNode(node->children[0].get(), env);

    for (auto &[ce, body] : node->branches) {
        auto cv = execNode(ce.get(), env);
        bool matched = false;

        if (cv.isCell()) {
            // Cell case: match if any element matches
            for (size_t i = 0; i < cv.numel() && !matched; ++i) {
                const auto &elem = cv.cellAt(i);
                if (sv.isNumeric() && elem.isNumeric()) {
                    if (sv.isScalar() && elem.isScalar())
                        matched = sv.toScalar() == elem.toScalar();
                } else if (sv.isChar() && elem.isChar()) {
                    matched = sv.toString() == elem.toString();
                } else if (sv.isLogical() && elem.isLogical()) {
                    if (sv.isScalar() && elem.isScalar())
                        matched = sv.toBool() == elem.toBool();
                }
            }
        } else {
            // Scalar case: direct comparison
            if (sv.isNumeric() && cv.isNumeric()) {
                if (sv.isScalar() && cv.isScalar())
                    matched = sv.toScalar() == cv.toScalar();
            } else if (sv.isChar() && cv.isChar()) {
                matched = sv.toString() == cv.toString();
            } else if (sv.isLogical() && cv.isLogical()) {
                if (sv.isScalar() && cv.isScalar())
                    matched = sv.toBool() == cv.toBool();
            }
        }

        if (matched)
            return execNode(body.get(), env);
    }

    if (node->elseBranch)
        return execNode(node->elseBranch.get(), env);
    return MValue::empty();
}

// ============================================================
// Function definition
// ============================================================
MValue Engine::execFunctionDef(const ASTNode *node, std::shared_ptr<Environment> env)
{
    UserFunction func;
    func.name = node->strValue;
    func.params = node->paramNames;
    func.returns = node->returnNames;
    // Clone the body into a shared_ptr for storage in userFuncs_
    func.body = std::shared_ptr<const ASTNode>(cloneNode(node->children[0].get()));
    func.closureEnv = nullptr;
    userFuncs_[func.name] = std::move(func);
    return MValue::empty();
}

MValue Engine::execExprStmt(const ASTNode *node, std::shared_ptr<Environment> env)
{
    auto val = execNode(node->children[0].get(), env);
    if (!node->suppressOutput && !val.isEmpty()) {
        env->set("ans", val);
        displayValue("ans", val);
    }
    return val;
}

// ============================================================
// Anonymous functions
// ============================================================
MValue Engine::execAnonFunc(const ASTNode *node, std::shared_ptr<Environment> env)
{
    if (!node->strValue.empty() && node->children.empty())
        return MValue::funcHandle(node->strValue, &allocator_);

    int id = anonCounter_.fetch_add(1, std::memory_order_relaxed);
    std::string anonName = "__anon_" + std::to_string(id);

    UserFunction uf;
    uf.name = anonName;
    uf.params = node->paramNames;
    uf.returns = {"__result__"};

    // Build body block: __result__ = <cloned expression>
    auto bodyBlock = std::make_shared<ASTNode>(NodeType::BLOCK);

    auto assignNode = std::make_unique<ASTNode>(NodeType::ASSIGN);
    auto resultId = std::make_unique<ASTNode>(NodeType::IDENTIFIER);
    resultId->strValue = "__result__";
    assignNode->children.push_back(std::move(resultId));
    assignNode->children.push_back(cloneNode(node->children[0].get()));
    assignNode->suppressOutput = true;

    bodyBlock->children.push_back(std::move(assignNode));
    uf.body = std::move(bodyBlock);

    uf.closureEnv = env->snapshot(globalEnv_, &globalStore_);

    userFuncs_[anonName] = std::move(uf);
    return MValue::funcHandle(anonName, &allocator_);
}

// ============================================================
// try/catch
// ============================================================
MValue Engine::execTryCatch(const ASTNode *node, std::shared_ptr<Environment> env)
{
    try {
        return execNode(node->children[0].get(), env);
    } catch (const BreakSignal &) {
        throw;
    } catch (const ContinueSignal &) {
        throw;
    } catch (const ReturnSignal &) {
        throw;
    } catch (const std::exception &e) {
        if (node->children.size() > 1) {
            if (!node->strValue.empty()) {
                auto err = MValue::structure();
                err.field("message") = MValue::fromString(e.what(), &allocator_);
                err.field("identifier") = MValue::fromString("MLAB:error", &allocator_);
                env->set(node->strValue, err);
            }
            return execNode(node->children[1].get(), env);
        }
        return MValue::empty();
    }
}

// ============================================================
// global / persistent
// ============================================================
MValue Engine::execGlobalPersistent(const ASTNode *node, std::shared_ptr<Environment> env)
{
    for (auto &name : node->paramNames) {
        env->declareGlobal(name);
        if (!globalStore_.get(name))
            globalStore_.set(name, MValue::empty());
    }
    return MValue::empty();
}

// ============================================================
// User function calls
// ============================================================
MValue Engine::callUserFunction(const UserFunction &func,
                                const std::vector<MValue> &args,
                                std::shared_ptr<Environment> env)
{
    auto results = callUserFunctionMulti(func, args, env, std::max(func.returns.size(), size_t(1)));
    return results.empty() ? MValue::empty() : results[0];
}

std::vector<MValue> Engine::callUserFunctionMulti(const UserFunction &func,
                                                  const std::vector<MValue> &args,
                                                  std::shared_ptr<Environment> env,
                                                  size_t nout)
{
    RecursionGuard rguard(currentRecursionDepth_, maxRecursionDepth_);

    auto parentEnv = func.closureEnv ? func.closureEnv : globalEnv_;
    auto localEnv = std::make_shared<Environment>(parentEnv, &globalStore_);

    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i)
        localEnv->setLocal(func.params[i], args[i]);

    localEnv->setLocal("nargin", MValue::scalar(static_cast<double>(args.size()), &allocator_));
    localEnv->setLocal("nargout", MValue::scalar(static_cast<double>(nout), &allocator_));

    for (auto &retName : func.returns)
        if (!localEnv->getLocal(retName))
            localEnv->setLocal(retName, MValue::empty());

    try {
        execNode(func.body.get(), localEnv);
    } catch (ReturnSignal &) {
        // Normal return
    }

    std::vector<MValue> results;
    results.reserve(std::min(func.returns.size(), nout));
    for (size_t i = 0; i < func.returns.size() && i < nout; ++i) {
        auto *val = localEnv->getLocal(func.returns[i]);
        if (!val)
            val = localEnv->get(func.returns[i]);
        results.push_back(val ? *val : MValue::empty());
    }
    return results;
}

bool Engine::tryBuiltinCall(const std::string &name,
                            const std::vector<MValue> &args,
                            std::shared_ptr<Environment> env,
                            MValue &result)
{
    if (name == "clear") {
        if (args.empty()) {
            // clear all local variables
            auto names = env->localNames();
            for (auto &n : names)
                env->remove(n);
        } else {
            for (auto &a : args) {
                if (a.isChar())
                    env->remove(a.toString());
            }
        }
        result = MValue::empty();
        return true;
    }

    if (name == "who") {
        std::ostringstream os;
        os << "Your variables are:\n";
        auto names = env->localNames();
        std::sort(names.begin(), names.end());
        for (auto &n : names) {
            if (n == "ans" || n == "nargin" || n == "nargout")
                continue;
            os << "  " << n << "\n";
        }
        output(os.str());
        result = MValue::empty();
        return true;
    }

    if (name == "whos") {
        std::ostringstream os;
        os << "  Name              Size            Bytes  Class\n";
        auto names = env->localNames();
        std::sort(names.begin(), names.end());
        for (auto &n : names) {
            if (n == "ans" || n == "nargin" || n == "nargout")
                continue;
            auto *val = env->get(n);
            if (!val)
                continue;
            auto &d = val->dims();
            std::string sizeStr = std::to_string(d.rows()) + "x" + std::to_string(d.cols());
            if (d.is3D())
                sizeStr += "x" + std::to_string(d.pages());
            os << "  " << n;
            for (size_t i = n.size(); i < 18; ++i)
                os << " ";
            os << sizeStr;
            for (size_t i = sizeStr.size(); i < 16; ++i)
                os << " ";
            os << val->rawBytes();
            for (size_t pad = std::to_string(val->rawBytes()).size(); pad < 7; ++pad)
                os << " ";
            os << mtypeName(val->type()) << "\n";
        }
        output(os.str());
        result = MValue::empty();
        return true;
    }

    if (name == "exist") {
        if (args.empty())
            throw std::runtime_error("exist requires a name argument");
        std::string varName = args[0].toString();
        double code = 0;
        if (env->has(varName))
            code = 1; // variable
        else if (externalFuncs_.count(varName) || userFuncs_.count(varName))
            code = 5; // function (built-in or user)
        result = MValue::scalar(code, &allocator_);
        return true;
    }

    if (name == "class") {
        if (args.empty())
            throw std::runtime_error("class requires an argument");
        result = MValue::fromString(mtypeName(args[0].type()), &allocator_);
        return true;
    }

    return false;
}

} // namespace mlab
