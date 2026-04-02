// src/MLabEngine.cpp
#include "MLabEngine.hpp"
#include "MLabCompiler.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"
#include "MLabStdLibrary.hpp"
#include "MLabTreeWalker.hpp"
#include "MLabVM.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace mlab {

// ============================================================
// Built-in constant names (shared — declared extern in MLabEngine.hpp)
// ============================================================
const std::unordered_set<std::string> kBuiltinNames = {"pi",
                                                       "eps",
                                                       "inf",
                                                       "Inf",
                                                       "nan",
                                                       "NaN",
                                                       "true",
                                                       "false",
                                                       "i",
                                                       "j",
                                                       "ans",
                                                       "nargin",
                                                       "nargout",
                                                       "end"};

// ============================================================
// Construction
// ============================================================
Engine::Engine()
{
    allocator_ = Allocator::defaultAllocator();
    globalEnv_ = std::make_unique<Environment>(nullptr, &globalStore_);
    treeWalker_ = std::make_unique<TreeWalker>(*this);
    compiler_ = std::make_unique<Compiler>(*this);
    vm_ = std::make_unique<VM>(*this);

    reinstallConstants();
    StdLibrary::install(*this);
}

Engine::~Engine() = default;

void Engine::reinstallConstants()
{
    globalEnv_->set("pi", MValue::scalar(3.14159265358979323846, &allocator_));
    globalEnv_->set("eps", MValue::scalar(2.2204460492503131e-16, &allocator_));
    globalEnv_->set("inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    globalEnv_->set("Inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    globalEnv_->set("nan", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    globalEnv_->set("NaN", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    globalEnv_->set("true", MValue::logicalScalar(true, &allocator_));
    globalEnv_->set("false", MValue::logicalScalar(false, &allocator_));
    globalEnv_->set("i", MValue::complexScalar(0.0, 1.0, &allocator_));
    globalEnv_->set("j", MValue::complexScalar(0.0, 1.0, &allocator_));
}

// ============================================================
// Registration & accessors
// ============================================================
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
    treeWalker_->setMaxRecursionDepth(d);
}

void Engine::outputText(const std::string &s)
{
    if (outputFunc_)
        outputFunc_(s);
    else
        std::cout << s;
}

bool Engine::hasFunction(const std::string &name) const
{
    static const std::unordered_set<std::string> kBuiltinFuncs = {"tic", "toc"};
    return externalFuncs_.count(name) || userFuncs_.count(name) || kBuiltinFuncs.count(name);
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

    if (backend_ != Backend::TreeWalker) {
        try {
            auto chunk = compiler_->compile(ast.get());
            vm_->setCompiledFuncs(&compiler_->compiledFuncs());
            MValue result = vm_->execute(chunk);

            // Export script-level variables to global environment
            for (auto &[name, val] : vm_->lastVarMap())
                if (!kBuiltinNames.count(name))
                    globalEnv_->set(name, val);

            return result;
        } catch (...) {
            // VM compilation/execution failed — fallback to TreeWalker
        }
    }

    return treeWalker_->execute(ast.get(), globalEnv_.get());
}

// ============================================================
// REPL helpers
// ============================================================
std::vector<std::string> Engine::globalVarNames() const
{
    auto names = globalEnv_->localNames();
    std::vector<std::string> result;
    result.reserve(names.size());
    for (auto &n : names)
        if (kBuiltinNames.count(n) == 0)
            result.push_back(n);
    std::sort(result.begin(), result.end());
    return result;
}

static std::string jsonEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}

std::string Engine::workspaceJSON() const
{
    auto names = globalVarNames();
    std::ostringstream os;
    os << "{";
    bool first = true;
    for (auto &name : names) {
        auto *val = globalEnv_->get(name);
        if (!val)
            continue;
        if (!first)
            os << ",";
        first = false;
        os << "\"" << jsonEscape(name) << "\":{";
        os << "\"type\":\"" << mtypeName(val->type()) << "\"";
        auto &d = val->dims();
        os << ",\"size\":\"" << d.rows() << "x" << d.cols();
        if (d.is3D())
            os << "x" << d.pages();
        os << "\"";
        os << ",\"bytes\":" << val->rawBytes();
        os << ",\"preview\":";
        if (val->type() == MType::DOUBLE && val->isScalar()) {
            double v = val->toScalar();
            if (std::isnan(v))
                os << "\"NaN\"";
            else if (std::isinf(v))
                os << (v > 0 ? "\"Inf\"" : "\"-Inf\"");
            else
                os << v;
        } else if (val->type() == MType::COMPLEX && val->isScalar()) {
            auto c = val->toComplex();
            os << "\"" << c.real();
            if (c.imag() >= 0)
                os << "+";
            os << c.imag() << "i\"";
        } else if (val->type() == MType::CHAR) {
            os << "\"" << jsonEscape(val->toString()) << "\"";
        } else if (val->type() == MType::LOGICAL && val->isScalar()) {
            os << (val->toBool() ? "true" : "false");
        } else if ((val->type() == MType::DOUBLE) && val->numel() <= 10) {
            os << "[";
            for (size_t i = 0; i < val->numel(); ++i) {
                if (i)
                    os << ",";
                os << val->doubleData()[i];
            }
            os << "]";
        } else {
            os << "null";
        }
        os << "}";
    }
    os << "}";
    return os.str();
}

} // namespace mlab