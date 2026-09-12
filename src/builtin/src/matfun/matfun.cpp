// src/builtin/src/matfun/matfun.cpp
//
// Matrix functions implementations for nk::builtin.

#include <nk/builtin/matfun.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>
#include <nk/builtin/ops.hpp>
#include <nk/builtin/datatypes.hpp>
#include <nk/builtin/elfun.hpp>

#include <cctype>
#include <stdexcept>
#include <string>

namespace nk::builtin {

Value idivide(const Value &a, const Value &b, const std::string &mode, std::pmr::memory_resource *mr)
{
    std::string opt = mode;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (opt.empty()) opt = "fix";

    const ValueType t0 = a.type();
    const ValueType t1 = b.type();
    const bool int0 = isIntegerType(t0);
    const bool int1 = isIntegerType(t1);
    const bool dbl0 = (t0 == ValueType::DOUBLE);
    const bool dbl1 = (t1 == ValueType::DOUBLE);

    if (!int0 && !int1)
        throw std::runtime_error("At least one argument must belong to an integer class.");

    ValueType resultType;
    if (int0 && int1) {
        if (t0 != t1)
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t0;
    } else if (int0) {
        if (!dbl1 || !b.isScalar())
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t0;
    } else {
        if (!dbl0 || !a.isScalar())
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t1;
    }

    const Value da = nk::builtin::toDouble(a, mr);
    const Value db = nk::builtin::toDouble(b, mr);
    Value q = nk::builtin::rdivide(da, db, mr);

    if (opt == "fix")          q = nk::builtin::fix(q, mr);
    else if (opt == "floor")  q = nk::builtin::floor(q, mr);
    else if (opt == "ceil")   q = nk::builtin::ceil(q, mr);
    else if (opt == "round")  q = nk::builtin::round(q, mr);
    else
        throw std::runtime_error("idivide: opt must be 'fix', 'floor', 'ceil', or 'round'");

    return nk::builtin::cast(q, mtypeName(resultType), mr);
}

} // namespace nk::builtin
