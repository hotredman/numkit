#include <numkit/core/shape_ops.hpp>
#include <numkit/core/value.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace numkit {



HeapObject Value::sEmptyTag;
HeapObject Value::sLogicalTrue;
HeapObject Value::sLogicalFalse;
HeapObject Value::sDeletedTag;
const Dims Value::sScalarDims{1, 1};
const Dims Value::sEmptyDims{};

Value Value::deleted()
{
    Value v;
    v.heap_ = deletedTag();
    return v;
}
Value::Value()
    : scalar_(0.0)
    , heap_(emptyTag())
{}
Value::~Value()
{
    releaseHeap();
}
Value::Value(const Value &o)
    : scalar_(o.scalar_)
    , heap_(o.heap_)
{
    if (o.isHeap())
        heap_->addRef();
}
Value &Value::operator=(const Value &o)
{
    if (this != &o) {
        releaseHeap();
        scalar_ = o.scalar_;
        heap_ = o.heap_;
        if (isHeap())
            heap_->addRef();
    }
    return *this;
}
Value::Value(Value &&o) noexcept
    : scalar_(o.scalar_)
    , heap_(o.heap_)
{
    o.heap_ = emptyTag();
    o.scalar_ = 0.0;
}
Value &Value::operator=(Value &&o) noexcept
{
    if (this != &o) {
        releaseHeap();
        scalar_ = o.scalar_;
        heap_ = o.heap_;
        o.heap_ = emptyTag();
        o.scalar_ = 0.0;
    }
    return *this;
}
void Value::swap(Value &o) noexcept
{
    std::swap(scalar_, o.scalar_);
    std::swap(heap_, o.heap_);
}
void Value::releaseHeap()
{
    if (isHeap()) {
        if (heap_->release())
            delete heap_;
    }
    heap_ = emptyTag();
}
void Value::detach()
{
    if (!isHeap())
        return;
    if (heap_->refCount.load(std::memory_order_acquire) <= 1)
        return;
    HeapObject *c = heap_->clone();
    releaseHeap();
    heap_ = c;
}
HeapObject *Value::mutableHeap()
{
    detach();
    return heap_;
}

Value Value::scalar(double v, std::pmr::memory_resource *)
{
    Value m;
    m.scalar_ = v;
    m.heap_ = nullptr;
    return m;
}
Value Value::logicalScalar(bool v, std::pmr::memory_resource *)
{
    Value m;
    m.heap_ = v ? logicalTrueTag() : logicalFalseTag();
    return m;
}
Value Value::empty()
{
    return Value();
}

Value Value::matrix(size_t rows, size_t cols, ValueType t, std::pmr::memory_resource *mr)
{
    if (rows == 1 && cols == 1 && t == ValueType::DOUBLE)
        return scalar(0.0, mr);
    Value m;
    auto *h = new HeapObject();
    h->type = t;
    h->dims = {rows, cols};
    h->mr = mr;
    size_t bytes = rows * cols * elementSize(t);
    if (bytes > 0) {
        h->buffer = new DataBuffer(bytes, mr);
        std::memset(h->buffer->data(), 0, bytes);
    }
    m.heap_ = h;
    return m;
}
Value Value::matrix3d(size_t rows, size_t cols, size_t pages, ValueType t, std::pmr::memory_resource *mr)
{
    Value m;
    auto *h = new HeapObject();
    h->type = t;
    h->dims = {rows, cols, pages};
    h->mr = mr;
    size_t bytes = rows * cols * pages * elementSize(t);
    if (bytes > 0) {
        h->buffer = new DataBuffer(bytes, mr);
        std::memset(h->buffer->data(), 0, bytes);
    }
    m.heap_ = h;
    return m;
}
Value Value::matrixND(const size_t *dims, int nd, ValueType t, std::pmr::memory_resource *mr)
{
    if (nd <= 0) return empty();
    if (nd == 1)
        return matrix(dims[0], 1, t, mr);
    if (nd == 2)
        return matrix(dims[0], dims[1], t, mr);
    if (nd == 3)
        return matrix3d(dims[0], dims[1], dims[2], t, mr);
    // nd >= 4 — go through the ND Dims ctor. Heap allocation in Dims only
    // kicks in for nd > kInlineCap (4); 4D stays fully inline.
    Value m;
    auto *h = new HeapObject();
    h->type = t;
    h->dims = Dims(dims, nd);
    h->mr = mr;
    size_t bytes = elementSize(t);
    for (int i = 0; i < nd; ++i)
        bytes *= dims[i];
    if (bytes > 0) {
        h->buffer = new DataBuffer(bytes, mr);
        std::memset(h->buffer->data(), 0, bytes);
    }
    m.heap_ = h;
    return m;
}
Value Value::fromString(const std::string &s, std::pmr::memory_resource *mr)
{
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::CHAR;
    h->dims = {1, s.size()};
    h->mr = mr;
    if (!s.empty()) {
        h->buffer = new DataBuffer(s.size(), mr);
        std::memcpy(h->buffer->data(), s.data(), s.size());
    }
    m.heap_ = h;
    return m;
}
Value Value::cell(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::CELL;
    h->dims = {rows, cols};
    h->mr = mr;
    h->cellData = new std::pmr::vector<Value>(rows * cols, mr);
    m.heap_ = h;
    return m;
}
Value Value::cell3D(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::CELL;
    h->dims = {rows, cols, pages};
    h->mr = mr;
    h->cellData = new std::pmr::vector<Value>(rows * cols * pages, mr);
    m.heap_ = h;
    return m;
}
Value Value::cellND(const size_t *dims, int nd, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    if (nd <= 0) return cell(0, 0, mr);
    if (nd == 1) return cell(dims[0], 1, mr);
    if (nd == 2) return cell(dims[0], dims[1], mr);
    if (nd == 3) return cell3D(dims[0], dims[1], dims[2], mr);
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::CELL;
    h->dims = Dims(dims, nd);
    h->mr = mr;
    size_t total = 1;
    for (int i = 0; i < nd; ++i) total *= dims[i];
    h->cellData = new std::pmr::vector<Value>(total, mr);
    m.heap_ = h;
    return m;
}
Value Value::stringScalar(const std::string &s, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::STRING;
    h->dims = {1, 1};
    h->mr = mr;
    h->cellData = new std::pmr::vector<Value>(1, Value::fromString(s, mr), mr);
    m.heap_ = h;
    return m;
}
Value Value::stringArray(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::STRING;
    h->dims = {rows, cols};
    h->mr = mr;
    // Initialize with empty strings
    h->cellData = new std::pmr::vector<Value>(rows * cols, Value::fromString("", mr), mr);
    m.heap_ = h;
    return m;
}
Value Value::stringArray3D(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::STRING;
    h->dims = {rows, cols, pages};
    h->mr = mr;
    h->cellData = new std::pmr::vector<Value>(rows * cols * pages,
                                               Value::fromString("", mr), mr);
    m.heap_ = h;
    return m;
}
const std::string &Value::stringElem(size_t i) const
{
    static const std::string empty;
    if (!heap_ || heap_->type != ValueType::STRING || !heap_->cellData || i >= heap_->cellData->size())
        return empty;
    // Each element is a CHAR Value — return its string representation cached nowhere,
    // so we use a thread_local buffer.
    // Actually, toString() returns by value, so let's store in the cellData as char MValues
    // and return from a static. Better: just return toString() but that returns by value.
    // For efficiency, return from a thread-local.
    thread_local std::string buf;
    buf = (*heap_->cellData)[i].toString();
    return buf;
}
void Value::stringElemSet(size_t i, const std::string &s)
{
    detach();
    if (i >= heap_->cellData->size())
        heap_->cellData->resize(i + 1, Value::fromString("", heap_->mr));
    (*heap_->cellData)[i] = Value::fromString(s, heap_->mr);
}
Value Value::structure(std::pmr::memory_resource *mr)
{
    if (!mr) mr = std::pmr::get_default_resource();
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::STRUCT;
    h->dims = {1, 1};
    h->mr = mr;
    h->structData = new std::pmr::map<std::string, Value>(mr);
    m.heap_ = h;
    return m;
}
Value Value::funcHandle(const std::string &name, std::pmr::memory_resource *mr)
{
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::FUNC_HANDLE;
    h->dims = {1, 1};
    h->mr = mr;
    h->funcName = new std::string(name);
    m.heap_ = h;
    return m;
}
// ============================================================
// Colon range: start:step:stop → row vector
// ============================================================
size_t Value::colonCount(double start, double step, double stop)
{
    if (std::isnan(start) || std::isnan(step) || std::isnan(stop))
        return 0; // MATLAB: NaN in colon produces empty
    if (std::isinf(start) || std::isinf(stop))
        throw std::runtime_error("Maximum variable size allowed by the program is exceeded");
    if (step == 0.0)
        throw std::runtime_error("Colon step cannot be zero");
    if (std::isinf(step))
        return (step > 0 && stop >= start) || (step < 0 && stop <= start) ? 1 : 0;
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
    return static_cast<size_t>(n);
}

Value Value::colonRange(double start, double stop, std::pmr::memory_resource *mr)
{
    return colonRange(start, 1.0, stop, mr);
}

Value Value::colonRange(double start, double step, double stop, std::pmr::memory_resource *mr)
{
    size_t count = Value::colonCount(start, step, stop);
    auto result = Value::matrix(1, count, ValueType::DOUBLE, mr);
    if (count > 0) {
        double *d = result.doubleDataMut();
        for (size_t i = 0; i < count; ++i)
            d[i] = start + static_cast<double>(i) * step;
        if (count >= 2) {
            double last = start + static_cast<double>(count - 1) * step;
            if ((step > 0 && last > stop) || (step < 0 && last < stop))
                d[count - 1] = stop;
        }
    }
    return result;
}

// ============================================================
// Concat helpers: type promotion and element access
// ============================================================

// Type promotion order: LOGICAL → DOUBLE → COMPLEX.
// Integer types (INT8..UINT64) are declared but not yet creatable at runtime;
// when they are, add them here with appropriate promotion rules.
static ValueType promoteNumericType(const Value *elems, size_t count)
{
    bool hasDouble = false;
    bool hasComplex = false;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        switch (elems[i].type()) {
        case ValueType::COMPLEX:
            hasComplex = true;
            break;
        case ValueType::DOUBLE:
            hasDouble = true;
            break;
        case ValueType::LOGICAL:
            break;
        default:
            throw std::runtime_error(
                std::string("Concatenation not supported for type '")
                + mtypeName(elems[i].type()) + "'");
        }
    }
    if (hasComplex)
        return ValueType::COMPLEX;
    if (hasDouble)
        return ValueType::DOUBLE;
    return ValueType::LOGICAL; // all-logical stays logical
}

// Read one element as double. Supports every numeric ValueType plus CHAR
// (returns the character's ASCII value) and LOGICAL (0/1). COMPLEX
// contributes the real part only, matching MATLAB's double(complex).
double Value::elemAsDouble(size_t idx) const
{
    switch (type()) {
    case ValueType::DOUBLE:  return doubleData()[idx];
    case ValueType::LOGICAL: return static_cast<double>(logicalData()[idx]);
    case ValueType::COMPLEX: return complexData()[idx].real();
    case ValueType::CHAR:
        return static_cast<double>(static_cast<unsigned char>(charData()[idx]));
    case ValueType::SINGLE:  return static_cast<double>(singleData()[idx]);
    case ValueType::INT8:    return static_cast<double>(int8Data()[idx]);
    case ValueType::INT16:   return static_cast<double>(int16Data()[idx]);
    case ValueType::INT32:   return static_cast<double>(int32Data()[idx]);
    case ValueType::INT64:   return static_cast<double>(int64Data()[idx]);
    case ValueType::UINT8:   return static_cast<double>(uint8Data()[idx]);
    case ValueType::UINT16:  return static_cast<double>(uint16Data()[idx]);
    case ValueType::UINT32:  return static_cast<double>(uint32Data()[idx]);
    case ValueType::UINT64:  return static_cast<double>(uint64Data()[idx]);
    default:
        throw std::runtime_error(
            std::string("Cannot read element as double from type '")
            + mtypeName(type()) + "'");
    }
}

// Read one element as Complex. Real-only types contribute zero
// imaginary part, matching MATLAB's real→complex promotion rule.
static Complex readElemAsComplex(const Value &v, size_t idx)
{
    switch (v.type()) {
    case ValueType::COMPLEX:
        return v.complexData()[idx];
    case ValueType::DOUBLE:
    case ValueType::LOGICAL:
    case ValueType::CHAR:
    case ValueType::SINGLE:
    case ValueType::INT8: case ValueType::INT16: case ValueType::INT32: case ValueType::INT64:
    case ValueType::UINT8: case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
        return Complex(v.elemAsDouble(idx), 0.0);
    default:
        throw std::runtime_error(
            std::string("Cannot read element as complex from type '")
            + mtypeName(v.type()) + "'");
    }
}

// Read one element as uint8_t logical. Every real numeric type is
// accepted — any non-zero is true. Matches MATLAB so concatenation
// of mixed-type arrays into a logical result behaves uniformly.
static uint8_t readElemAsLogical(const Value &v, size_t idx)
{
    switch (v.type()) {
    case ValueType::LOGICAL:
        return v.logicalData()[idx];
    case ValueType::DOUBLE:
    case ValueType::CHAR:
    case ValueType::SINGLE:
    case ValueType::INT8: case ValueType::INT16: case ValueType::INT32: case ValueType::INT64:
    case ValueType::UINT8: case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
        return v.elemAsDouble(idx) != 0.0 ? 1 : 0;
    case ValueType::COMPLEX:
        return (v.complexData()[idx] != Complex(0.0, 0.0)) ? 1 : 0;
    default:
        throw std::runtime_error(
            std::string("Cannot read element as logical from type '")
            + mtypeName(v.type()) + "'");
    }
}

// Get dimensions for a concat element: rows, cols, pages.
// Scalars are treated as 1×1×1.
static void getElemDims(const Value &v, size_t &r, size_t &c, size_t &p)
{
    if (v.isScalar()) {
        r = c = p = 1;
    } else {
        auto &d = v.dims();
        r = d.rows();
        c = d.cols();
        p = d.is3D() ? d.pages() : 1;
    }
}

// Validate and accumulate one dimension (rows or pages) that must match across elements.
// First non-empty element sets the value; subsequent must agree (broadcast 1 allowed).
static void matchDim(size_t &out, size_t elem, bool &set)
{
    if (!set) {
        out = elem;
        set = true;
    } else if (out != elem && elem != 1 && out != 1) {
        throw std::runtime_error(
            "Dimensions of arrays being concatenated are not consistent");
    }
    if (elem > 1)
        out = elem;
}

// Copy elements from source into a column-major destination buffer.
// Template avoids duplicating the loop structure for double vs Complex.
template<typename T, typename ReadFunc>
static void copyBlock(T *dst, size_t dstRows, size_t dstCols,
                      const Value &src, size_t srcRows, size_t srcCols, size_t srcPages,
                      size_t rowOff, size_t colOff, size_t pages,
                      ReadFunc read)
{
    size_t dstSlice = dstRows * dstCols;
    size_t srcSlice = srcRows * srcCols;
    for (size_t p = 0; p < pages; ++p)
        for (size_t c = 0; c < srcCols; ++c)
            for (size_t r = 0; r < srcRows; ++r)
                dst[(rowOff + r) + (colOff + c) * dstRows + p * dstSlice] =
                    read(src, r + c * srcRows + p * srcSlice);
}

// ============================================================
// Horizontal concatenation: [a, b, c]
// Concatenates along dimension 2 (columns).
// Rows and pages must match across all elements.
// ============================================================
Value Value::horzcat(const Value *elems, size_t count, std::pmr::memory_resource *mr)
{
    // String array horzcat: ["a", "b", "c"] → 1×3 string
    bool hasString = false;
    for (size_t i = 0; i < count; ++i)
        if (!elems[i].isEmpty() && elems[i].type() == ValueType::STRING) {
            hasString = true;
            break;
        }
    if (hasString) {
        size_t totalCols = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].isString())
                totalCols += elems[i].dims().cols();
            else
                totalCols += 1; // scalar char or other → one string element
        }
        auto result = Value::stringArray(1, totalCols);
        size_t pos = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].isString()) {
                for (size_t j = 0; j < elems[i].numel(); ++j)
                    result.stringElemSet(pos++, elems[i].stringElem(j));
            } else {
                result.stringElemSet(pos++, elems[i].toString());
            }
        }
        return result;
    }

    // Char concatenation path
    bool hasChar = false;
    for (size_t i = 0; i < count; ++i)
        if (!elems[i].isEmpty() && elems[i].type() == ValueType::CHAR) {
            hasChar = true;
            break;
        }

    if (hasChar) {
        std::string result;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].type() == ValueType::CHAR) {
                result += elems[i].toString();
            } else if (elems[i].isScalar()) {
                result += static_cast<char>(static_cast<int>(std::round(elems[i].toScalar())));
            } else {
                // Non-scalar double/logical: convert each element to char
                const double *d = elems[i].doubleData();
                for (size_t k = 0; k < elems[i].numel(); ++k)
                    result += static_cast<char>(static_cast<int>(std::round(d[k])));
            }
        }
        return Value::fromString(result, mr);
    }

    // Collect dimensions
    size_t totalCols = 0, rows = 0, pages = 1;
    bool rowsSet = false, pagesSet = false;

    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);
        totalCols += eC;
        matchDim(rows, eR, rowsSet);
        matchDim(pages, eP, pagesSet);
    }
    if (totalCols == 0)
        return Value::empty();
    if (!rows)
        rows = 1;

    ValueType outType = promoteNumericType(elems, count);

    auto result = (pages > 1) ? Value::matrix3d(rows, totalCols, pages, outType, mr)
                              : Value::matrix(rows, totalCols, outType, mr);

    size_t colOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);

        if (outType == ValueType::COMPLEX)
            copyBlock(result.complexDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, readElemAsComplex);
        else if (outType == ValueType::LOGICAL)
            copyBlock(result.logicalDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, readElemAsLogical);
        else
            copyBlock(result.doubleDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, [](const Value &v, size_t i) { return v.elemAsDouble(i); });
        colOff += eC;
    }
    return result;
}

// ============================================================
// Vertical concatenation: [a; b; c]
// Concatenates along dimension 1 (rows).
// Columns and pages must match across all elements.
// ============================================================
Value Value::vertcat(const Value *elems, size_t count, std::pmr::memory_resource *mr)
{
    size_t totalRows = 0, cols = 0, pages = 1;
    bool colsSet = false, pagesSet = false;
    bool hasCell = false;

    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        if (elems[i].isCell())
            hasCell = true;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);
        totalRows += eR;
        matchDim(cols, eC, colsSet);
        matchDim(pages, eP, pagesSet);
    }
    if (!cols)
        return Value::empty();

    // Char vertcat
    bool hasChar = false;
    for (size_t i = 0; i < count; ++i)
        if (!elems[i].isEmpty() && elems[i].type() == ValueType::CHAR) {
            hasChar = true;
            break;
        }
    if (hasChar) {
        auto result = Value::matrix(totalRows, cols, ValueType::CHAR, mr);
        char *dst = result.charDataMut();
        size_t rowOff = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            size_t eR, eC, eP;
            getElemDims(elems[i], eR, eC, eP);
            if (elems[i].type() == ValueType::CHAR) {
                const char *src = elems[i].charData();
                for (size_t c = 0; c < eC; ++c)
                    for (size_t r = 0; r < eR; ++r)
                        dst[c * totalRows + rowOff + r] = src[c * eR + r];
            } else {
                // Double/logical to char conversion
                for (size_t c = 0; c < eC; ++c)
                    for (size_t r = 0; r < eR; ++r)
                        dst[c * totalRows + rowOff + r] =
                            static_cast<char>(static_cast<int>(std::round(
                                elems[i].elemAsDouble(c * eR + r))));
            }
            rowOff += eR;
        }
        return result;
    }

    // Cell vertcat: combine rows into a 2D cell (column-major)
    if (hasCell) {
        auto result = Value::cell(totalRows, cols);
        size_t rowOff = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            size_t eR = elems[i].dims().rows();
            size_t eC = elems[i].dims().cols();
            for (size_t c = 0; c < eC; ++c)
                for (size_t r = 0; r < eR; ++r)
                    result.cellAt((c) * totalRows + (rowOff + r)) =
                        elems[i].cellAt(c * eR + r);
            rowOff += eR;
        }
        return result;
    }

    ValueType outType = promoteNumericType(elems, count);

    auto result = (pages > 1) ? Value::matrix3d(totalRows, cols, pages, outType, mr)
                              : Value::matrix(totalRows, cols, outType, mr);

    size_t rowOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);

        if (outType == ValueType::COMPLEX)
            copyBlock(result.complexDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, readElemAsComplex);
        else if (outType == ValueType::LOGICAL)
            copyBlock(result.logicalDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, readElemAsLogical);
        else
            copyBlock(result.doubleDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, [](const Value &v, size_t i) { return v.elemAsDouble(i); });
        rowOff += eR;
    }
    return result;
}

// ============================================================
// Type-preserving indexing
// ============================================================

// Helper: create a scalar Value of the same type as this array at linear index.
Value Value::elemAt(size_t idx, std::pmr::memory_resource *mr) const
{
    ValueType t = type();
    switch (t) {
    case ValueType::DOUBLE:
        return Value::scalar(doubleData()[idx], mr);
    case ValueType::COMPLEX:
        return Value::complexScalar(complexData()[idx], mr);
    case ValueType::LOGICAL:
        return Value::logicalScalar(logicalData()[idx] != 0, mr);
    case ValueType::CHAR: {
        std::string s(1, charData()[idx]);
        return Value::fromString(s, mr);
    }
    case ValueType::CELL:
        return cellAt(idx);
    // Typed integer / single return a 1×1 array of the same type so the
    // user's type sticks through indexing. Raw memcpy from the source
    // buffer keeps the representation byte-exact.
    case ValueType::INT8: case ValueType::INT16: case ValueType::INT32: case ValueType::INT64:
    case ValueType::UINT8: case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
    case ValueType::SINGLE: {
        size_t es = elementSize(t);
        Value r = Value::matrix(1, 1, t, mr);
        std::memcpy(r.rawDataMut(),
                    static_cast<const char *>(rawData()) + idx * es, es);
        return r;
    }
    default:
        throw std::runtime_error(
            std::string("elemAt not supported for type '") + mtypeName(t) + "'");
    }
}

// 1D slice: extract elements at given linear indices.
// Shape rule: column vector source → column result; otherwise → row result.
// CELL: always returns sub-cell (even for count==1), matching MATLAB c(i) semantics.
Value Value::indexGet(const size_t *indices, size_t count, std::pmr::memory_resource *mr) const
{
    // For non-CELL scalar result, return a scalar value
    if (count == 1 && type() != ValueType::CELL)
        return elemAt(indices[0], mr);

    // Shape: column vector source → column result
    bool colResult = (dims().cols() == 1 && dims().rows() > 1);
    size_t rr = colResult ? count : 1;
    size_t cc = colResult ? 1 : count;

    ValueType t = type();
    switch (t) {
    case ValueType::DOUBLE: {
        auto result = Value::matrix(rr, cc, ValueType::DOUBLE, mr);
        double *dst = result.doubleDataMut();
        const double *src = doubleData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case ValueType::COMPLEX: {
        auto result = Value::complexMatrix(rr, cc, mr);
        Complex *dst = result.complexDataMut();
        const Complex *src = complexData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case ValueType::LOGICAL: {
        auto result = Value::matrix(rr, cc, ValueType::LOGICAL, mr);
        uint8_t *dst = result.logicalDataMut();
        const uint8_t *src = logicalData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case ValueType::CHAR: {
        std::string s;
        s.reserve(count);
        const char *src = charData();
        for (size_t i = 0; i < count; ++i)
            s += src[indices[i]];
        return Value::fromString(s, mr);
    }
    case ValueType::CELL: {
        auto result = Value::cell(rr, cc);
        for (size_t i = 0; i < count; ++i)
            result.cellAt(i) = cellAt(indices[i]);
        return result;
    }
    // Typed integer / single: raw memcpy per element, same target type.
    case ValueType::INT8: case ValueType::INT16: case ValueType::INT32: case ValueType::INT64:
    case ValueType::UINT8: case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
    case ValueType::SINGLE: {
        size_t es = elementSize(t);
        auto result = Value::matrix(rr, cc, t, mr);
        const char *src = static_cast<const char *>(rawData());
        char *dst = static_cast<char *>(result.rawDataMut());
        for (size_t i = 0; i < count; ++i)
            std::memcpy(dst + i * es, src + indices[i] * es, es);
        return result;
    }
    default:
        throw std::runtime_error(
            std::string("indexGet not supported for type '") + mtypeName(t) + "'");
    }
}

// 2D slice: extract sub-matrix at given row/col indices.
Value Value::indexGet2D(const size_t *rowIdx, size_t nrows,
                          const size_t *colIdx, size_t ncols,
                          std::pmr::memory_resource *mr) const
{
    // Scalar shortcut for non-CELL types
    if (nrows == 1 && ncols == 1 && type() != ValueType::CELL) {
        size_t idx = dims().sub2ind(rowIdx[0], colIdx[0]);
        return elemAt(idx, mr);
    }

    ValueType t = type();

    if (t == ValueType::CELL) {
        auto &d = dims();
        auto result = Value::cell(nrows, ncols);
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                result.cellAt(c * nrows + r) = cellAt(d.sub2ind(rowIdx[r], colIdx[c]));
        return result;
    }

    size_t es = elementSize(t);
    if (es == 0)
        throw std::runtime_error(
            std::string("indexGet2D not supported for type '") + mtypeName(t) + "'");

    auto &d = dims();
    auto result = Value::matrix(nrows, ncols, t, mr);
    const char *src = static_cast<const char *>(rawData());
    char *dst = static_cast<char *>(result.rawDataMut());
    for (size_t c = 0; c < ncols; ++c)
        for (size_t r = 0; r < nrows; ++r)
            std::memcpy(dst + (c * nrows + r) * es,
                        src + d.sub2ind(rowIdx[r], colIdx[c]) * es, es);
    return result;
}

// 3D slice: extract sub-array at given row/col/page indices.
Value Value::indexGet3D(const size_t *rowIdx, size_t nrows,
                          const size_t *colIdx, size_t ncols,
                          const size_t *pageIdx, size_t npages,
                          std::pmr::memory_resource *mr) const
{
    // Scalar shortcut for non-CELL types
    if (nrows == 1 && ncols == 1 && npages == 1 && type() != ValueType::CELL) {
        size_t idx = dims().sub2ind(rowIdx[0], colIdx[0], pageIdx[0]);
        return elemAt(idx, mr);
    }

    ValueType t = type();

    if (t == ValueType::CELL) {
        auto &d = dims();
        auto result = Value::cell3D(nrows, ncols, npages);
        Dims rd(nrows, ncols, npages);
        for (size_t p = 0; p < npages; ++p)
            for (size_t c = 0; c < ncols; ++c)
                for (size_t r = 0; r < nrows; ++r)
                    result.cellAt(rd.sub2ind(r, c, p)) =
                        cellAt(d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]));
        return result;
    }

    size_t es = elementSize(t);
    if (es == 0)
        throw std::runtime_error(
            std::string("indexGet3D not supported for type '") + mtypeName(t) + "'");

    auto &d = dims();
    auto result = Value::matrix3d(nrows, ncols, npages, t, mr);
    const char *src = static_cast<const char *>(rawData());
    char *dst = static_cast<char *>(result.rawDataMut());
    Dims rd(nrows, ncols, npages);
    for (size_t p = 0; p < npages; ++p)
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                std::memcpy(dst + rd.sub2ind(r, c, p) * es,
                            src + d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]) * es, es);
    return result;
}

// ND slice: extract sub-tensor at given per-dim indices.
Value Value::indexGetND(const size_t *const *perDimIdx,
                          const size_t *perDimCount,
                          int nd,
                          std::pmr::memory_resource *mr) const
{
    if (nd <= 0) return empty();
    if (nd == 1) return indexGet(perDimIdx[0], perDimCount[0], mr);
    if (nd == 2)
        return indexGet2D(perDimIdx[0], perDimCount[0],
                          perDimIdx[1], perDimCount[1], mr);
    if (nd == 3)
        return indexGet3D(perDimIdx[0], perDimCount[0],
                          perDimIdx[1], perDimCount[1],
                          perDimIdx[2], perDimCount[2], mr);

    ValueType t = type();
    const bool isCell = (t == ValueType::CELL);
    size_t es = isCell ? 0 : elementSize(t);
    if (!isCell && es == 0)
        throw std::runtime_error(
            std::string("indexGetND not supported for type '") + mtypeName(t) + "'");

    // Build output shape from perDimCount; trailing zero-extent dims
    // produce an empty tensor.
    size_t totalOut = 1;
    for (int i = 0; i < nd; ++i) totalOut *= perDimCount[i];
    auto result = isCell ? Value::cellND(perDimCount, nd)
                         : Value::matrixND(perDimCount, nd, t, mr);
    if (totalOut == 0) return result;

    // Source strides (column-major) for the existing tensor's actual rank.
    auto &srcDims = dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    size_t srcStrides[kMaxNd];
    if (srcDims.ndim() > kMaxNd)
        throw std::runtime_error("indexGetND: source rank exceeds 32");
    computeStridesColMajor(srcDims, srcStrides);
    const int srcNd = srcDims.ndim();

    // Bounds check: for any dim i picked from perDimIdx[i], idx < srcDims.dim(i).
    // For dims i beyond the source's actual ndim, the source is implicitly
    // extended with trailing singletons — only idx == 0 is valid.
    for (int i = 0; i < nd; ++i) {
        const size_t lim = (i < srcNd) ? srcDims.dim(i) : 1;
        for (size_t k = 0; k < perDimCount[i]; ++k)
            if (perDimIdx[i][k] >= lim)
                throw std::runtime_error(
                    "Index exceeds array dimensions (dim " + std::to_string(i + 1)
                    + ": " + std::to_string(perDimIdx[i][k] + 1) + " > "
                    + std::to_string(lim) + ")");
    }

    Dims outIter(perDimCount, nd);
    size_t coords[kMaxNd] = {0};
    size_t outIdx = 0;
    if (isCell) {
        do {
            size_t srcOff = 0;
            const int loopN = std::min(nd, srcNd);
            for (int i = 0; i < loopN; ++i) {
                const size_t pickedIdx = perDimIdx[i][coords[i]];
                srcOff += pickedIdx * srcStrides[i];
            }
            result.cellAt(outIdx) = cellAt(srcOff);
            ++outIdx;
        } while (incrementCoords(coords, outIter));
    } else {
        const char *src = static_cast<const char *>(rawData());
        char *dst = static_cast<char *>(result.rawDataMut());
        do {
            size_t srcOff = 0;
            const int loopN = std::min(nd, srcNd);
            for (int i = 0; i < loopN; ++i) {
                const size_t pickedIdx = perDimIdx[i][coords[i]];
                srcOff += pickedIdx * srcStrides[i];
            }
            // Dims i ≥ srcNd contribute 0 (already verified by bounds check).
            std::memcpy(dst + outIdx * es, src + srcOff * es, es);
            ++outIdx;
        } while (incrementCoords(coords, outIter));
    }

    return result;
}

// Logical indexing: extract elements where mask is true → row vector of same type.
Value Value::logicalIndex(const uint8_t *mask, size_t maskLen, std::pmr::memory_resource *mr) const
{
    // Check for true values beyond array bounds
    for (size_t i = numel(); i < maskLen; ++i)
        if (mask[i])
            throw std::runtime_error("Index exceeds the number of array elements");
    size_t n = std::min(maskLen, numel());
    // Count selected elements
    size_t selected = 0;
    for (size_t i = 0; i < n; ++i)
        if (mask[i])
            selected++;

    // Shape: column vector source → column result
    bool colResult = (dims().cols() == 1 && dims().rows() > 1);
    size_t rr = colResult ? selected : 1;
    size_t cc = colResult ? 1 : selected;

    ValueType t = type();
    switch (t) {
    case ValueType::DOUBLE: {
        auto result = Value::matrix(rr, cc, ValueType::DOUBLE, mr);
        double *dst = result.doubleDataMut();
        const double *src = doubleData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case ValueType::COMPLEX: {
        auto result = Value::complexMatrix(rr, cc, mr);
        Complex *dst = result.complexDataMut();
        const Complex *src = complexData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case ValueType::LOGICAL: {
        auto result = Value::matrix(rr, cc, ValueType::LOGICAL, mr);
        uint8_t *dst = result.logicalDataMut();
        const uint8_t *src = logicalData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case ValueType::CHAR: {
        auto result = Value::matrix(rr, cc, ValueType::CHAR, mr);
        char *dst = result.charDataMut();
        const char *src = charData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case ValueType::CELL: {
        auto result = Value::cell(rr, cc);
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                result.cellAt(k++) = cellAt(i);
        return result;
    }
    // Typed integer / single: raw memcpy per selected element.
    case ValueType::INT8: case ValueType::INT16: case ValueType::INT32: case ValueType::INT64:
    case ValueType::UINT8: case ValueType::UINT16: case ValueType::UINT32: case ValueType::UINT64:
    case ValueType::SINGLE: {
        size_t es = elementSize(t);
        auto result = Value::matrix(rr, cc, t, mr);
        const char *src = static_cast<const char *>(rawData());
        char *dst = static_cast<char *>(result.rawDataMut());
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                std::memcpy(dst + k++ * es, src + i * es, es);
        return result;
    }
    default:
        throw std::runtime_error(
            std::string("logicalIndex not supported for type '") + mtypeName(t) + "'");
    }
}

// ============================================================
// Index resolution — convert Value index to vector<size_t> (0-based)
// ============================================================

static void validateIndex(double v)
{
    if (std::isnan(v) || std::isinf(v) || v < 1.0 || v != std::floor(v))
        throw std::runtime_error("Array indices must be positive integers or logical values");
}

std::vector<size_t> Value::resolveIndices(const Value &idx, size_t dimSize)
{
    std::vector<size_t> out;
    if (idx.isChar() && idx.numel() == 1 && idx.charData()[0] == ':') {
        out.resize(dimSize);
        for (size_t i = 0; i < dimSize; ++i)
            out[i] = i;
    } else if (idx.isLogical()) {
        const uint8_t *m = idx.logicalData();
        for (size_t i = dimSize; i < idx.numel(); ++i)
            if (m[i])
                throw std::runtime_error("Index exceeds the number of array elements");
        size_t n = std::min(idx.numel(), dimSize);
        for (size_t i = 0; i < n; ++i)
            if (m[i])
                out.push_back(i);
    } else if (idx.isDoubleScalar()) {
        double v = idx.toScalar();
        validateIndex(v);
        size_t ii = static_cast<size_t>(v) - 1;
        if (ii >= dimSize)
            throw std::runtime_error("Index exceeds array dimensions");
        out.push_back(ii);
    } else {
        const double *d = idx.doubleData();
        for (size_t i = 0; i < idx.numel(); ++i) {
            validateIndex(d[i]);
            size_t ii = static_cast<size_t>(d[i]) - 1;
            if (ii >= dimSize)
                throw std::runtime_error("Index exceeds array dimensions");
            out.push_back(ii);
        }
    }
    return out;
}

std::vector<size_t> Value::resolveIndicesUnchecked(const Value &idx)
{
    std::vector<size_t> out;
    if (idx.isChar() && idx.numel() == 1 && idx.charData()[0] == ':') {
        // Colon in unchecked context — caller must handle this separately
        return out;
    }
    if (idx.isLogical()) {
        const uint8_t *m = idx.logicalData();
        for (size_t i = 0; i < idx.numel(); ++i)
            if (m[i])
                out.push_back(i);
    } else if (idx.isDoubleScalar() || idx.isLogicalScalar()) {
        double v = idx.toScalar();
        validateIndex(v);
        out.push_back(static_cast<size_t>(v) - 1);
    } else {
        const double *d = idx.doubleData();
        for (size_t i = 0; i < idx.numel(); ++i) {
            validateIndex(d[i]);
            out.push_back(static_cast<size_t>(d[i]) - 1);
        }
    }
    return out;
}

// ============================================================
// Type-preserving indexed assignment
// ============================================================

// Helper: write one element from val into dst array at linear index.
// Converts val to the destination type. COW detach handled by *DataMut().
static void writeElem(Value &dst, size_t idx, const Value &val, size_t valIdx)
{
    ValueType t = dst.type();
    switch (t) {
    case ValueType::DOUBLE:
        dst.doubleDataMut()[idx] = val.elemAsDouble(valIdx);
        break;
    case ValueType::COMPLEX:
        dst.complexDataMut()[idx] = readElemAsComplex(val, valIdx);
        break;
    case ValueType::LOGICAL: {
        double dv = val.elemAsDouble(valIdx);
        dst.logicalDataMut()[idx] = static_cast<uint8_t>(dv != 0.0);
        break;
    }
    case ValueType::CHAR:
        dst.charDataMut()[idx] = static_cast<char>(static_cast<int>(val.elemAsDouble(valIdx)));
        break;
    case ValueType::CELL:
        dst.cellAt(idx) = val.cellAt(valIdx);
        break;
    // Typed integer / single: narrowing conversion from the source
    // element's double value. MATLAB saturates on overflow; we match
    // that by using the cast boundaries of each target type.
    case ValueType::INT8:   dst.int8DataMut()[idx]   = static_cast<int8_t  >(val.elemAsDouble(valIdx)); break;
    case ValueType::INT16:  dst.int16DataMut()[idx]  = static_cast<int16_t >(val.elemAsDouble(valIdx)); break;
    case ValueType::INT32:  dst.int32DataMut()[idx]  = static_cast<int32_t >(val.elemAsDouble(valIdx)); break;
    case ValueType::INT64:  dst.int64DataMut()[idx]  = static_cast<int64_t >(val.elemAsDouble(valIdx)); break;
    case ValueType::UINT8:  dst.uint8DataMut()[idx]  = static_cast<uint8_t >(val.elemAsDouble(valIdx)); break;
    case ValueType::UINT16: dst.uint16DataMut()[idx] = static_cast<uint16_t>(val.elemAsDouble(valIdx)); break;
    case ValueType::UINT32: dst.uint32DataMut()[idx] = static_cast<uint32_t>(val.elemAsDouble(valIdx)); break;
    case ValueType::UINT64: dst.uint64DataMut()[idx] = static_cast<uint64_t>(val.elemAsDouble(valIdx)); break;
    case ValueType::SINGLE: dst.singleDataMut()[idx] = static_cast<float   >(val.elemAsDouble(valIdx)); break;
    default:
        throw std::runtime_error(
            std::string("Indexed assignment not supported for type '")
            + mtypeName(t) + "'");
    }
}

// Write a scalar val (broadcast) into dst at linear index.
static void writeScalar(Value &dst, size_t idx, const Value &val)
{
    ValueType t = dst.type();
    switch (t) {
    case ValueType::DOUBLE:
        dst.doubleDataMut()[idx] = val.toScalar();
        break;
    case ValueType::COMPLEX:
        dst.complexDataMut()[idx] = val.toComplex();
        break;
    case ValueType::LOGICAL:
        dst.logicalDataMut()[idx] = static_cast<uint8_t>(val.toScalar() != 0.0);
        break;
    case ValueType::CHAR:
        dst.charDataMut()[idx] = static_cast<char>(static_cast<int>(val.toScalar()));
        break;
    case ValueType::CELL:
        dst.cellAt(idx) = val;
        break;
    case ValueType::INT8:   dst.int8DataMut()[idx]   = static_cast<int8_t  >(val.toScalar()); break;
    case ValueType::INT16:  dst.int16DataMut()[idx]  = static_cast<int16_t >(val.toScalar()); break;
    case ValueType::INT32:  dst.int32DataMut()[idx]  = static_cast<int32_t >(val.toScalar()); break;
    case ValueType::INT64:  dst.int64DataMut()[idx]  = static_cast<int64_t >(val.toScalar()); break;
    case ValueType::UINT8:  dst.uint8DataMut()[idx]  = static_cast<uint8_t >(val.toScalar()); break;
    case ValueType::UINT16: dst.uint16DataMut()[idx] = static_cast<uint16_t>(val.toScalar()); break;
    case ValueType::UINT32: dst.uint32DataMut()[idx] = static_cast<uint32_t>(val.toScalar()); break;
    case ValueType::UINT64: dst.uint64DataMut()[idx] = static_cast<uint64_t>(val.toScalar()); break;
    case ValueType::SINGLE: dst.singleDataMut()[idx] = static_cast<float   >(val.toScalar()); break;
    default:
        throw std::runtime_error(
            std::string("Indexed assignment not supported for type '")
            + mtypeName(t) + "'");
    }
}

void Value::elemSet(size_t idx, const Value &val)
{
    // Promote double→complex if assigning complex value
    if (type() == ValueType::DOUBLE && val.isComplex())
        promoteToComplex();
    writeScalar(*this, idx, val);
}

void Value::indexSet(const size_t *indices, size_t count, const Value &val)
{
    // Promote double→complex if assigning complex value
    if (type() == ValueType::DOUBLE && val.isComplex())
        promoteToComplex();
    if (val.isScalar()) {
        for (size_t k = 0; k < count; ++k)
            writeScalar(*this, indices[k], val);
    } else {
        if (count != val.numel())
            throw std::runtime_error(
                "Unable to perform assignment because the left and right sides have a different number of elements");
        for (size_t k = 0; k < count; ++k)
            writeElem(*this, indices[k], val, k);
    }
}

void Value::indexSet2D(const size_t *rowIdx, size_t nrows,
                        const size_t *colIdx, size_t ncols,
                        const Value &val)
{
    if (type() == ValueType::DOUBLE && val.isComplex())
        promoteToComplex();
    auto &d = dims();
    if (val.isScalar()) {
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                writeScalar(*this, d.sub2ind(rowIdx[r], colIdx[c]), val);
    } else {
        size_t total = nrows * ncols;
        if (total != val.numel())
            throw std::runtime_error(
                "Unable to perform assignment because the left and right sides have a different number of elements");
        size_t k = 0;
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                writeElem(*this, d.sub2ind(rowIdx[r], colIdx[c]), val, k++);
    }
}

void Value::indexSet3D(const size_t *rowIdx, size_t nrows,
                        const size_t *colIdx, size_t ncols,
                        const size_t *pageIdx, size_t npages,
                        const Value &val)
{
    if (type() == ValueType::DOUBLE && val.isComplex())
        promoteToComplex();
    auto &d = dims();
    if (val.isScalar()) {
        for (size_t p = 0; p < npages; ++p)
            for (size_t c = 0; c < ncols; ++c)
                for (size_t r = 0; r < nrows; ++r)
                    writeScalar(*this, d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]), val);
    } else {
        size_t total = nrows * ncols * npages;
        if (total != val.numel())
            throw std::runtime_error(
                "Unable to perform assignment because the left and right sides have a different number of elements");
        size_t k = 0;
        for (size_t p = 0; p < npages; ++p)
            for (size_t c = 0; c < ncols; ++c)
                for (size_t r = 0; r < nrows; ++r)
                    writeElem(*this, d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]), val, k++);
    }
}

void Value::indexSetND(const size_t *const *perDimIdx,
                        const size_t *perDimCount,
                        int nd,
                        const Value &val)
{
    if (nd <= 0) return;
    if (nd == 1) { indexSet(perDimIdx[0], perDimCount[0], val); return; }
    if (nd == 2) {
        indexSet2D(perDimIdx[0], perDimCount[0],
                   perDimIdx[1], perDimCount[1], val);
        return;
    }
    if (nd == 3) {
        indexSet3D(perDimIdx[0], perDimCount[0],
                   perDimIdx[1], perDimCount[1],
                   perDimIdx[2], perDimCount[2], val);
        return;
    }

    // CELL is handled element-wise via writeScalar/writeElem (which use
    // cellAt under the hood) — no special-casing needed here past the
    // shared scalar/elementwise loops below.
    if (type() == ValueType::DOUBLE && val.isComplex())
        promoteToComplex();

    constexpr int kMaxNd = Dims::kMaxRank;
    if (dims().ndim() > kMaxNd)
        throw std::runtime_error("indexSetND: target rank exceeds 32");

    // Auto-grow: expand any axis whose largest assignment index exceeds
    // current dim, including new trailing axes (rank goes up). Matches
    // MATLAB: `A(i,j,k,l) = v` creates / extends as needed.
    {
        const int curNd = dims().ndim();
        const int newNd = std::max(nd, curNd);
        if (newNd > kMaxNd)
            throw std::runtime_error("indexSetND: target rank exceeds 32");
        size_t need[kMaxNd];
        for (int i = 0; i < newNd; ++i)
            need[i] = (i < curNd) ? dims().dim(i) : 1;
        bool grow = false;
        for (int i = 0; i < nd; ++i) {
            for (size_t k = 0; k < perDimCount[i]; ++k) {
                if (perDimIdx[i][k] + 1 > need[i]) {
                    need[i] = perDimIdx[i][k] + 1;
                    grow = true;
                }
            }
        }
        if (grow)
            resizeND(need, newNd, nullptr);
    }

    auto &d = dims();
    size_t dstStrides[kMaxNd];
    computeStridesColMajor(d, dstStrides);
    const int dstNd = d.ndim();

    size_t total = 1;
    for (int i = 0; i < nd; ++i) total *= perDimCount[i];
    if (total == 0) return;

    Dims iter(perDimCount, nd);
    size_t coords[kMaxNd] = {0};

    if (val.isScalar()) {
        do {
            size_t off = 0;
            const int loopN = std::min(nd, dstNd);
            for (int i = 0; i < loopN; ++i)
                off += perDimIdx[i][coords[i]] * dstStrides[i];
            writeScalar(*this, off, val);
        } while (incrementCoords(coords, iter));
    } else {
        if (total != val.numel())
            throw std::runtime_error(
                "Unable to perform assignment because the left and right sides have a different number of elements");
        size_t k = 0;
        do {
            size_t off = 0;
            const int loopN = std::min(nd, dstNd);
            for (int i = 0; i < loopN; ++i)
                off += perDimIdx[i][coords[i]] * dstStrides[i];
            writeElem(*this, off, val, k++);
        } while (incrementCoords(coords, iter));
    }
}

// ============================================================
// Type-preserving index deletion
// ============================================================

void Value::indexDelete(const size_t *indices, size_t count, std::pmr::memory_resource *mr)
{
    ValueType t = type();
    // STRING elements are std::string, not fixed-width bytes, so the
    // memcpy-based path below would silently produce zero-sized output
    // (elementSize(STRING) == 0). Reject up-front alongside the other
    // structurally-incompatible types.
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE || t == ValueType::EMPTY
        || t == ValueType::STRING)
        throw std::runtime_error(
            std::string("Delete indexing not supported for type '") + mtypeName(t) + "'");

    size_t n = numel();
    std::vector<bool> del(n, false);
    for (size_t k = 0; k < count; ++k)
        if (indices[k] < n)
            del[indices[k]] = true;

    size_t remaining = std::count(del.begin(), del.end(), false);
    // MATLAB: 1D delete always produces row vector, except column vectors stay column
    bool isRow = !(dims().cols() == 1 && dims().rows() > 1);

    if (t == ValueType::CELL) {
        auto &src = cellDataVec();
        auto result = isRow ? Value::cell(1, remaining) : Value::cell(remaining, 1);
        auto &dst = result.cellDataVec();
        size_t j = 0;
        for (size_t i = 0; i < n; ++i)
            if (!del[i])
                dst[j++] = src[i];
        *this = std::move(result);
        return;
    }

    size_t es = elementSize(t);
    auto result = isRow ? Value::matrix(1, remaining, t, mr)
                        : Value::matrix(remaining, 1, t, mr);
    if (remaining > 0 && es > 0) {
        const char *src = static_cast<const char *>(rawData());
        char *dst = static_cast<char *>(result.rawDataMut());
        size_t j = 0;
        for (size_t i = 0; i < n; ++i)
            if (!del[i])
                std::memcpy(dst + j++ * es, src + i * es, es);
    }
    *this = std::move(result);
}

void Value::indexDelete2D(const size_t *rowIdx, size_t nrows,
                           const size_t *colIdx, size_t ncols,
                           std::pmr::memory_resource *mr)
{
    ValueType t = type();
    // STRING elements are std::string, not fixed-width bytes, so the
    // memcpy-based path below would silently produce zero-sized output
    // (elementSize(STRING) == 0). Reject up-front alongside the other
    // structurally-incompatible types.
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE || t == ValueType::EMPTY
        || t == ValueType::STRING)
        throw std::runtime_error(
            std::string("Delete indexing not supported for type '") + mtypeName(t) + "'");

    size_t R = dims().rows(), C = dims().cols();

    if (ncols == C) {
        // Delete rows
        std::vector<bool> delR(R, false);
        for (size_t k = 0; k < nrows; ++k)
            if (rowIdx[k] < R)
                delR[rowIdx[k]] = true;
        size_t newR = std::count(delR.begin(), delR.end(), false);

        if (t == ValueType::CELL) {
            auto &src = cellDataVec();
            auto result = Value::cell(newR, C);
            auto &dst = result.cellDataVec();
            size_t ri = 0;
            for (size_t r = 0; r < R; ++r)
                if (!delR[r]) {
                    for (size_t c = 0; c < C; ++c)
                        dst[c * newR + ri] = src[c * R + r];
                    ri++;
                }
            *this = std::move(result);
        } else {
            size_t es = elementSize(t);
            auto result = Value::matrix(newR, C, t, mr);
            if (newR > 0 && es > 0) {
                const char *src = static_cast<const char *>(rawData());
                char *dst = static_cast<char *>(result.rawDataMut());
                size_t ri = 0;
                for (size_t r = 0; r < R; ++r)
                    if (!delR[r]) {
                        for (size_t c = 0; c < C; ++c)
                            std::memcpy(dst + (c * newR + ri) * es,
                                        src + (c * R + r) * es, es);
                        ri++;
                    }
            }
            *this = std::move(result);
        }
    } else if (nrows == R) {
        // Delete columns
        std::vector<bool> delC(C, false);
        for (size_t k = 0; k < ncols; ++k)
            if (colIdx[k] < C)
                delC[colIdx[k]] = true;
        size_t newC = std::count(delC.begin(), delC.end(), false);

        if (t == ValueType::CELL) {
            auto &src = cellDataVec();
            auto result = Value::cell(R, newC);
            auto &dst = result.cellDataVec();
            size_t ci = 0;
            for (size_t c = 0; c < C; ++c)
                if (!delC[c]) {
                    for (size_t r = 0; r < R; ++r)
                        dst[ci * R + r] = src[c * R + r];
                    ci++;
                }
            *this = std::move(result);
        } else {
            size_t es = elementSize(t);
            auto result = Value::matrix(R, newC, t, mr);
            if (newC > 0 && es > 0) {
                const char *src = static_cast<const char *>(rawData());
                char *dst = static_cast<char *>(result.rawDataMut());
                size_t ci = 0;
                for (size_t c = 0; c < C; ++c)
                    if (!delC[c]) {
                        std::memcpy(dst + ci * R * es, src + c * R * es, R * es);
                        ci++;
                    }
            }
            *this = std::move(result);
        }
    } else {
        throw std::runtime_error(
            "Cannot delete from both rows and columns simultaneously");
    }
}

void Value::indexDelete3D(const size_t *rowIdx, size_t nrows,
                           const size_t *colIdx, size_t ncols,
                           const size_t *pageIdx, size_t npages,
                           std::pmr::memory_resource *mr)
{
    ValueType t = type();
    // STRING elements are std::string, not fixed-width bytes, so the
    // memcpy-based path below would silently produce zero-sized output
    // (elementSize(STRING) == 0). Reject up-front alongside the other
    // structurally-incompatible types.
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE || t == ValueType::EMPTY
        || t == ValueType::STRING)
        throw std::runtime_error(
            std::string("Delete indexing not supported for type '") + mtypeName(t) + "'");

    size_t R = dims().rows(), C = dims().cols(), P = dims().pages();

    // Exactly one dimension must be a proper subset; the other two must be fully selected
    bool fullR = (nrows == R), fullC = (ncols == C), fullP = (npages == P);
    int partialCount = (!fullR ? 1 : 0) + (!fullC ? 1 : 0) + (!fullP ? 1 : 0);
    if (partialCount != 1)
        throw std::runtime_error(
            "3D delete requires exactly one dimension to be partially selected");

    if (!fullP) {
        // Delete pages
        std::vector<bool> delP(P, false);
        for (size_t k = 0; k < npages; ++k)
            if (pageIdx[k] < P) delP[pageIdx[k]] = true;
        size_t newP = std::count(delP.begin(), delP.end(), false);

        if (t == ValueType::CELL) {
            auto result = Value::cell3D(R, C, newP);
            Dims rd(R, C, newP);
            size_t pi = 0;
            for (size_t p = 0; p < P; ++p)
                if (!delP[p]) {
                    for (size_t c = 0; c < C; ++c)
                        for (size_t r = 0; r < R; ++r)
                            result.cellAt(rd.sub2ind(r, c, pi)) =
                                cellAt(dims().sub2ind(r, c, p));
                    pi++;
                }
            *this = std::move(result);
        } else {
            size_t es = elementSize(t);
            auto result = Value::matrix3d(R, C, newP, t, mr);
            size_t sliceBytes = R * C * es;
            const char *src = static_cast<const char *>(rawData());
            char *dst = static_cast<char *>(result.rawDataMut());
            size_t pi = 0;
            for (size_t p = 0; p < P; ++p)
                if (!delP[p])
                    std::memcpy(dst + pi++ * sliceBytes, src + p * sliceBytes, sliceBytes);
            *this = std::move(result);
        }
    } else if (!fullR) {
        // Delete rows (across all pages)
        std::vector<bool> delR(R, false);
        for (size_t k = 0; k < nrows; ++k)
            if (rowIdx[k] < R) delR[rowIdx[k]] = true;
        size_t newR = std::count(delR.begin(), delR.end(), false);

        if (t == ValueType::CELL) {
            auto result = Value::cell3D(newR, C, P);
            Dims rd(newR, C, P);
            for (size_t p = 0; p < P; ++p) {
                size_t ri = 0;
                for (size_t r = 0; r < R; ++r)
                    if (!delR[r]) {
                        for (size_t c = 0; c < C; ++c)
                            result.cellAt(rd.sub2ind(ri, c, p)) =
                                cellAt(dims().sub2ind(r, c, p));
                        ri++;
                    }
            }
            *this = std::move(result);
        } else {
            size_t es = elementSize(t);
            auto result = Value::matrix3d(newR, C, P, t, mr);
            const char *src = static_cast<const char *>(rawData());
            char *dst = static_cast<char *>(result.rawDataMut());
            for (size_t p = 0; p < P; ++p) {
                size_t ri = 0;
                for (size_t r = 0; r < R; ++r)
                    if (!delR[r]) {
                        for (size_t c = 0; c < C; ++c)
                            std::memcpy(
                                dst + (p * newR * C + c * newR + ri) * es,
                                src + (p * R * C + c * R + r) * es, es);
                        ri++;
                    }
            }
            *this = std::move(result);
        }
    } else {
        // Delete columns (across all pages)
        std::vector<bool> delC(C, false);
        for (size_t k = 0; k < ncols; ++k)
            if (colIdx[k] < C) delC[colIdx[k]] = true;
        size_t newC = std::count(delC.begin(), delC.end(), false);

        if (t == ValueType::CELL) {
            auto result = Value::cell3D(R, newC, P);
            Dims rd(R, newC, P);
            for (size_t p = 0; p < P; ++p) {
                size_t ci = 0;
                for (size_t c = 0; c < C; ++c)
                    if (!delC[c]) {
                        for (size_t r = 0; r < R; ++r)
                            result.cellAt(rd.sub2ind(r, ci, p)) =
                                cellAt(dims().sub2ind(r, c, p));
                        ci++;
                    }
            }
            *this = std::move(result);
        } else {
            size_t es = elementSize(t);
            auto result = Value::matrix3d(R, newC, P, t, mr);
            const char *src = static_cast<const char *>(rawData());
            char *dst = static_cast<char *>(result.rawDataMut());
            for (size_t p = 0; p < P; ++p) {
                size_t ci = 0;
                for (size_t c = 0; c < C; ++c)
                    if (!delC[c]) {
                        std::memcpy(
                            dst + (p * R * newC + ci * R) * es,
                            src + (p * R * C + c * R) * es, R * es);
                        ci++;
                    }
            }
            *this = std::move(result);
        }
    }
}

void Value::indexDeleteND(const size_t *const *perDimIdx,
                           const size_t *perDimCount,
                           int nd,
                           std::pmr::memory_resource *mr)
{
    if (nd <= 0) return;
    if (nd == 1) { indexDelete(perDimIdx[0], perDimCount[0], mr); return; }
    if (nd == 2) {
        indexDelete2D(perDimIdx[0], perDimCount[0],
                      perDimIdx[1], perDimCount[1], mr);
        return;
    }
    if (nd == 3) {
        indexDelete3D(perDimIdx[0], perDimCount[0],
                      perDimIdx[1], perDimCount[1],
                      perDimIdx[2], perDimCount[2], mr);
        return;
    }

    ValueType t = type();
    if (t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE || t == ValueType::EMPTY
        || t == ValueType::STRING)
        throw std::runtime_error(
            std::string("Delete indexing not supported for type '") + mtypeName(t) + "'");

    auto &d = dims();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (d.ndim() > kMaxNd)
        throw std::runtime_error("indexDeleteND: target rank exceeds 32");
    const int srcNd = d.ndim();

    // Find the partial axis. All other axes must be the full range.
    int partial = -1;
    for (int i = 0; i < nd; ++i) {
        const size_t lim = (i < srcNd) ? d.dim(i) : 1;
        if (perDimCount[i] != lim) {
            if (partial != -1)
                throw std::runtime_error(
                    "ND delete requires exactly one dimension to be partially selected");
            partial = i;
        }
    }
    if (partial == -1)
        throw std::runtime_error(
            "ND delete requires exactly one dimension to be partially selected");
    if (partial >= srcNd)
        throw std::runtime_error(
            "ND delete: partial axis exceeds source rank");

    const size_t axisLen = d.dim(partial);
    std::vector<bool> delMask(axisLen, false);
    for (size_t k = 0; k < perDimCount[partial]; ++k)
        if (perDimIdx[partial][k] < axisLen)
            delMask[perDimIdx[partial][k]] = true;
    const size_t newLen = std::count(delMask.begin(), delMask.end(), false);

    size_t newShape[kMaxNd];
    for (int i = 0; i < srcNd; ++i)
        newShape[i] = (i == partial) ? newLen : d.dim(i);

    const bool isCell = (t == ValueType::CELL);
    const size_t es = isCell ? 0 : elementSize(t);
    if (!isCell && es == 0)
        throw std::runtime_error(
            std::string("indexDeleteND not supported for type '") + mtypeName(t) + "'");

    Value result = isCell ? Value::cellND(newShape, srcNd)
                           : Value::matrixND(newShape, srcNd, t, mr);
    if (newLen == 0) {
        *this = std::move(result);
        return;
    }

    size_t srcStrides[kMaxNd], dstStrides[kMaxNd];
    computeStridesColMajor(d, srcStrides);
    Dims newDims(newShape, srcNd);
    computeStridesColMajor(newDims, dstStrides);

    // Per-axis-value compressed-output index.
    std::vector<size_t> remap(axisLen, 0);
    {
        size_t j = 0;
        for (size_t i = 0; i < axisLen; ++i)
            if (!delMask[i]) remap[i] = j++;
    }

    char *dstRaw = isCell ? nullptr : static_cast<char *>(result.rawDataMut());
    const char *srcRaw = isCell ? nullptr : static_cast<const char *>(rawData());

    size_t coords[kMaxNd] = {0};
    do {
        if (delMask[coords[partial]])
            continue;
        size_t srcOff = 0, dstOff = 0;
        for (int i = 0; i < srcNd; ++i) {
            srcOff += coords[i] * srcStrides[i];
            const size_t dc = (i == partial) ? remap[coords[i]] : coords[i];
            dstOff += dc * dstStrides[i];
        }
        if (isCell)
            result.cellAt(dstOff) = cellAt(srcOff);
        else
            std::memcpy(dstRaw + dstOff * es, srcRaw + srcOff * es, es);
    } while (incrementCoords(coords, d));

    *this = std::move(result);
}

Value Value::complexScalar(Complex v, std::pmr::memory_resource *mr)
{
    Value m;
    auto *h = new HeapObject();
    h->type = ValueType::COMPLEX;
    h->dims = {1, 1};
    h->mr = mr;
    h->buffer = new DataBuffer(sizeof(Complex), mr);
    *static_cast<Complex *>(h->buffer->data()) = v;
    m.heap_ = h;
    return m;
}
Value Value::complexScalar(double re, double im, std::pmr::memory_resource *mr)
{
    return complexScalar(Complex(re, im), mr);
}
Value Value::complexMatrix(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    return matrix(rows, cols, ValueType::COMPLEX, mr);
}

ValueType Value::type() const
{
    if (heap_ == nullptr)
        return ValueType::DOUBLE;
    if (heap_ == emptyTag())
        return ValueType::EMPTY;
    if (heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return ValueType::LOGICAL;
    return heap_->type;
}
const Dims &Value::dims() const
{
    if (heap_ == nullptr)
        return sScalarDims;
    if (heap_ == emptyTag())
        return sEmptyDims;
    if (heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return sScalarDims;
    return heap_->dims;
}
size_t Value::numel() const
{
    return dims().numel();
}
bool Value::isScalar() const
{
    if (heap_ == nullptr || heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return true;
    if (heap_ == emptyTag())
        return false;
    return heap_->dims.isScalar();
}
bool Value::isEmpty() const
{
    if (heap_ == emptyTag())
        return true;
    if (heap_ == nullptr || heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return false;
    return heap_->type == ValueType::EMPTY || heap_->dims.isEmpty();
}
bool Value::isNumeric() const
{
    ValueType t = type();
    return t == ValueType::DOUBLE || t == ValueType::SINGLE || t == ValueType::COMPLEX || t == ValueType::INT8
           || t == ValueType::INT16 || t == ValueType::INT32 || t == ValueType::INT64 || t == ValueType::UINT8
           || t == ValueType::UINT16 || t == ValueType::UINT32 || t == ValueType::UINT64;
}
bool Value::isComplex() const
{
    return type() == ValueType::COMPLEX;
}
bool Value::isLogical() const
{
    return type() == ValueType::LOGICAL;
}
bool Value::isChar() const
{
    return type() == ValueType::CHAR;
}
bool Value::isCell() const
{
    return type() == ValueType::CELL;
}
bool Value::isStruct() const
{
    return type() == ValueType::STRUCT;
}
bool Value::isFuncHandle() const
{
    return type() == ValueType::FUNC_HANDLE;
}
bool Value::isString() const
{
    return type() == ValueType::STRING;
}

const void *Value::rawData() const
{
    if (heap_ == nullptr)
        return &scalar_;
    if (isTag())
        return nullptr;
    return heap_->buffer ? heap_->buffer->data() : nullptr;
}
size_t Value::rawBytes() const
{
    if (heap_ == nullptr)
        return sizeof(double);
    if (isTag())
        return 0;
    return heap_->buffer ? heap_->buffer->bytes() : 0;
}

const double *Value::doubleData() const
{
    if (heap_ == nullptr)
        return &scalar_;
    if (isTag())
        throw std::runtime_error("Not a double array");
    if (heap_->type != ValueType::DOUBLE)
        throw std::runtime_error("Not a double array");
    return heap_->buffer ? static_cast<const double *>(heap_->buffer->data()) : nullptr;
}
const uint8_t *Value::logicalData() const
{
    if (heap_ == logicalTrueTag()) {
        static const uint8_t t = 1;
        return &t;
    }
    if (heap_ == logicalFalseTag()) {
        static const uint8_t f = 0;
        return &f;
    }
    if (!isHeap() || heap_->type != ValueType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    return heap_->buffer ? static_cast<const uint8_t *>(heap_->buffer->data()) : nullptr;
}
const char *Value::charData() const
{
    if (!isHeap() || heap_->type != ValueType::CHAR)
        throw std::runtime_error("Not a char array");
    return heap_->buffer ? static_cast<const char *>(heap_->buffer->data()) : nullptr;
}
double Value::toScalar() const
{
    if (heap_ == nullptr)
        return scalar_;
    if (heap_ == logicalTrueTag())
        return 1.0;
    if (heap_ == logicalFalseTag())
        return 0.0;
    if (!isHeap() || !heap_->buffer)
        throw std::runtime_error("Cannot convert " + std::string(mtypeName(type())) + " to scalar");
    auto *h = heap_;
    if (h->type == ValueType::DOUBLE && h->dims.isScalar())
        return *static_cast<const double *>(h->buffer->data());
    if (h->type == ValueType::COMPLEX && h->dims.isScalar()) {
        auto c = *static_cast<const Complex *>(h->buffer->data());
        if (c.imag() != 0.0)
            throw std::runtime_error(
                "Cannot convert complex with nonzero imaginary part to double scalar");
        return c.real();
    }
    if (h->type == ValueType::LOGICAL && h->dims.isScalar())
        return (double) *static_cast<const uint8_t *>(h->buffer->data());
    if (h->type == ValueType::CHAR && h->dims.isScalar())
        return (double) (unsigned char) *static_cast<const char *>(h->buffer->data());
    if (h->type == ValueType::SINGLE && h->dims.isScalar())
        return (double) *static_cast<const float *>(h->buffer->data());
    if (h->type == ValueType::INT8 && h->dims.isScalar())
        return (double) *static_cast<const int8_t *>(h->buffer->data());
    if (h->type == ValueType::INT16 && h->dims.isScalar())
        return (double) *static_cast<const int16_t *>(h->buffer->data());
    if (h->type == ValueType::INT32 && h->dims.isScalar())
        return (double) *static_cast<const int32_t *>(h->buffer->data());
    if (h->type == ValueType::INT64 && h->dims.isScalar())
        return (double) *static_cast<const int64_t *>(h->buffer->data());
    if (h->type == ValueType::UINT8 && h->dims.isScalar())
        return (double) *static_cast<const uint8_t *>(h->buffer->data());
    if (h->type == ValueType::UINT16 && h->dims.isScalar())
        return (double) *static_cast<const uint16_t *>(h->buffer->data());
    if (h->type == ValueType::UINT32 && h->dims.isScalar())
        return (double) *static_cast<const uint32_t *>(h->buffer->data());
    if (h->type == ValueType::UINT64 && h->dims.isScalar())
        return (double) *static_cast<const uint64_t *>(h->buffer->data());
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type())) + " to scalar");
}
bool Value::toBool() const
{
    if (heap_ == nullptr)
        return scalar_ != 0.0;
    if (heap_ == logicalTrueTag())
        return true;
    if (heap_ == logicalFalseTag())
        return false;
    if (heap_ == emptyTag())
        throw std::runtime_error("Cannot convert empty to bool");
    auto *h = heap_;
    if (h->type == ValueType::LOGICAL && h->dims.isScalar() && h->buffer)
        return *static_cast<const uint8_t *>(h->buffer->data()) != 0;
    if (h->type == ValueType::DOUBLE && h->dims.isScalar() && h->buffer)
        return *static_cast<const double *>(h->buffer->data()) != 0.0;
    if (h->type == ValueType::COMPLEX && h->dims.isScalar() && h->buffer) {
        auto c = *static_cast<const Complex *>(h->buffer->data());
        return c.real() != 0.0 || c.imag() != 0.0;
    }
    if (h->type == ValueType::DOUBLE && h->buffer) {
        const double *dd = static_cast<const double *>(h->buffer->data());
        size_t n = h->dims.numel();
        for (size_t i = 0; i < n; ++i)
            if (dd[i] == 0.0)
                return false;
        return n > 0;
    }
    if (h->type == ValueType::LOGICAL && h->buffer) {
        const uint8_t *ld = static_cast<const uint8_t *>(h->buffer->data());
        size_t n = h->dims.numel();
        for (size_t i = 0; i < n; ++i)
            if (!ld[i])
                return false;
        return n > 0;
    }
    if (h->type == ValueType::COMPLEX && h->buffer) {
        const Complex *cd = static_cast<const Complex *>(h->buffer->data());
        size_t n = h->dims.numel();
        for (size_t i = 0; i < n; ++i)
            if (cd[i].real() == 0.0 && cd[i].imag() == 0.0)
                return false;
        return n > 0;
    }
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type())) + " to bool");
}
std::string Value::toString() const
{
    if (isHeap() && heap_->type == ValueType::CHAR) {
        if (heap_->buffer)
            return std::string(static_cast<const char *>(heap_->buffer->data()),
                               heap_->dims.numel());
        return std::string(); // empty string (1x0 char)
    }
    if (isHeap() && heap_->type == ValueType::STRING) {
        if (heap_->cellData && !heap_->cellData->empty())
            return (*heap_->cellData)[0].toString();
        return std::string();
    }
    if (isHeap() && heap_->type == ValueType::FUNC_HANDLE && heap_->funcName)
        return *heap_->funcName;
    throw std::runtime_error("Not a char array");
}
std::string Value::funcHandleName() const
{
    if (isHeap() && heap_->funcName)
        return *heap_->funcName;
    return "";
}

const Complex *Value::complexData() const
{
    if (!isHeap() || heap_->type != ValueType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    return heap_->buffer ? static_cast<const Complex *>(heap_->buffer->data()) : nullptr;
}
Complex Value::toComplex() const
{
    if (heap_ == nullptr)
        return Complex(scalar_, 0.0);
    if (heap_ == logicalTrueTag())
        return Complex(1.0, 0.0);
    if (heap_ == logicalFalseTag())
        return Complex(0.0, 0.0);
    if (!isHeap() || !heap_->buffer)
        throw std::runtime_error("Cannot convert to complex");
    if (heap_->type == ValueType::COMPLEX && heap_->dims.isScalar())
        return *static_cast<const Complex *>(heap_->buffer->data());
    if (heap_->dims.isScalar() && (isNumeric() || isLogical() || isChar()))
        return Complex(toScalar(), 0.0);
    throw std::runtime_error("Cannot convert to complex");
}
Complex Value::complexElem(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Complex index out of bounds");
    return complexData()[i];
}
Complex Value::complexElem(size_t r, size_t c) const
{
    return complexData()[dims().sub2indChecked(r, c)];
}

double *Value::doubleDataMut()
{
    if (heap_ == nullptr)
        return &scalar_;
    if (!isHeap() || heap_->type != ValueType::DOUBLE)
        throw std::runtime_error("Not a double array");
    detach();
    return heap_->buffer ? static_cast<double *>(heap_->buffer->data()) : nullptr;
}
uint8_t *Value::logicalDataMut()
{
    if (!isHeap() || heap_->type != ValueType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    detach();
    return heap_->buffer ? static_cast<uint8_t *>(heap_->buffer->data()) : nullptr;
}
char *Value::charDataMut()
{
    if (!isHeap() || heap_->type != ValueType::CHAR)
        throw std::runtime_error("Not a char array");
    detach();
    return heap_->buffer ? static_cast<char *>(heap_->buffer->data()) : nullptr;
}
void *Value::rawDataMut()
{
    if (heap_ == nullptr)
        return &scalar_;
    if (!isHeap())
        return nullptr;
    detach();
    return heap_->buffer ? heap_->buffer->data() : nullptr;
}
Complex *Value::complexDataMut()
{
    if (!isHeap() || heap_->type != ValueType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    detach();
    return heap_->buffer ? static_cast<Complex *>(heap_->buffer->data()) : nullptr;
}

const float *Value::singleData() const { return static_cast<const float*>(rawData()); }
float *Value::singleDataMut() { return static_cast<float*>(rawDataMut()); }
const int8_t *Value::int8Data() const { return static_cast<const int8_t*>(rawData()); }
int8_t *Value::int8DataMut() { return static_cast<int8_t*>(rawDataMut()); }
const int16_t *Value::int16Data() const { return static_cast<const int16_t*>(rawData()); }
int16_t *Value::int16DataMut() { return static_cast<int16_t*>(rawDataMut()); }
const int32_t *Value::int32Data() const { return static_cast<const int32_t*>(rawData()); }
int32_t *Value::int32DataMut() { return static_cast<int32_t*>(rawDataMut()); }
const int64_t *Value::int64Data() const { return static_cast<const int64_t*>(rawData()); }
int64_t *Value::int64DataMut() { return static_cast<int64_t*>(rawDataMut()); }
const uint8_t  *Value::uint8Data()  const { return static_cast<const uint8_t *>(rawData()); }
uint8_t  *Value::uint8DataMut()  { return static_cast<uint8_t *>(rawDataMut()); }
const uint16_t *Value::uint16Data() const { return static_cast<const uint16_t*>(rawData()); }
uint16_t *Value::uint16DataMut() { return static_cast<uint16_t*>(rawDataMut()); }
const uint32_t *Value::uint32Data() const { return static_cast<const uint32_t*>(rawData()); }
uint32_t *Value::uint32DataMut() { return static_cast<uint32_t*>(rawDataMut()); }
const uint64_t *Value::uint64Data() const { return static_cast<const uint64_t*>(rawData()); }
uint64_t *Value::uint64DataMut() { return static_cast<uint64_t*>(rawDataMut()); }

void Value::promoteToComplex(std::pmr::memory_resource *mr)
{
    ValueType t = type();
    if (t == ValueType::COMPLEX)
        return;
    if (t != ValueType::DOUBLE)
        throw std::runtime_error("Can only promote double to complex");
    size_t n = numel();
    if (heap_ == nullptr) {
        double v = scalar_;
        *this = complexScalar(Complex(v, 0.0), mr);
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot promote to complex");
    detach(); // COW: ensure we have our own copy before mutating
    if (!mr)
        mr = heap_->mr;
    auto *newBuf = new DataBuffer(n * sizeof(Complex), mr);
    Complex *dst = static_cast<Complex *>(newBuf->data());
    if (n > 0 && heap_->buffer) {
        const double *src = static_cast<const double *>(heap_->buffer->data());
        for (size_t i = 0; i < n; ++i)
            dst[i] = Complex(src[i], 0.0);
    }
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = newBuf;
    heap_->type = ValueType::COMPLEX;
}

double Value::operator()(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Index out of bounds");
    return doubleData()[i];
}
double Value::operator()(size_t r, size_t c) const
{
    return doubleData()[dims().sub2indChecked(r, c)];
}
double Value::operator()(size_t r, size_t c, size_t p) const
{
    return doubleData()[dims().sub2indChecked(r, c, p)];
}
double &Value::elem(size_t i)
{
    if (i >= numel())
        throw std::runtime_error("Index out of bounds");
    return doubleDataMut()[i];
}
double &Value::elem(size_t r, size_t c)
{
    return doubleDataMut()[dims().sub2indChecked(r, c)];
}
double &Value::elem(size_t r, size_t c, size_t p)
{
    return doubleDataMut()[dims().sub2indChecked(r, c, p)];
}

char Value::charElem(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Char index out of bounds");
    return charData()[i];
}
char &Value::charElemMut(size_t i)
{
    if (i >= numel())
        throw std::runtime_error("Char index out of bounds");
    return charDataMut()[i];
}

std::string Value::charRow(size_t r) const
{
    if (type() != ValueType::CHAR)
        throw std::runtime_error("charRow: value is not a char array");
    auto &d = dims();
    if (r >= d.rows())
        throw std::runtime_error("charRow: row index out of bounds");
    const char *cd = charData();
    size_t R = d.rows(), C = d.cols();
    std::string s;
    s.reserve(C);
    for (size_t c = 0; c < C; ++c)
        s += cd[c * R + r];
    return s;
}

void Value::resize(size_t newRows, size_t newCols, std::pmr::memory_resource *mr)
{
    if (heap_ == nullptr) {
        double v = scalar_;
        auto *h = new HeapObject();
        h->type = ValueType::DOUBLE;
        h->dims = {1, 1};
        h->mr = mr;
        h->buffer = new DataBuffer(sizeof(double), mr);
        *static_cast<double *>(h->buffer->data()) = v;
        heap_ = h;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot resize");
    if (heap_->dims.is3D()) {
        resize3d(newRows, newCols, heap_->dims.pages(), mr);
        return;
    }
    detach();
    if (!mr)
        mr = heap_->mr;
    size_t oldR = heap_->dims.rows(), oldC = heap_->dims.cols(), es = elementSize(heap_->type),
           nb = newRows * newCols * es;
    auto *nb2 = new DataBuffer(nb, mr);
    if (nb > 0) {
        // CHAR arrays fill with spaces; everything else with zeros
        int fill = (heap_->type == ValueType::CHAR) ? ' ' : 0;
        std::memset(nb2->data(), fill, nb);
    }
    if (heap_->buffer && es > 0) {
        size_t cr = std::min(oldR, newRows), cc = std::min(oldC, newCols);
        const char *s = static_cast<const char *>(heap_->buffer->data());
        char *d = static_cast<char *>(nb2->data());
        for (size_t c = 0; c < cc; ++c)
            std::memcpy(d + c * newRows * es, s + c * oldR * es, cr * es);
    }
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = nb2;
    heap_->mr = mr;
    heap_->dims = {newRows, newCols};
    heap_->appendCapacity = 0;
}

void Value::resize3d(size_t nr, size_t nc, size_t np, std::pmr::memory_resource *mr)
{
    if (np <= 1) {
        resize(nr, nc, mr);
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot resize");
    detach();
    if (!mr)
        mr = heap_->mr;
    size_t oR = heap_->dims.rows(), oC = heap_->dims.cols(), oP = heap_->dims.pages(),
           es = elementSize(heap_->type);
    size_t nb = nr * nc * np * es;
    auto *nb2 = new DataBuffer(nb, mr);
    if (nb > 0) {
        int fill = (heap_->type == ValueType::CHAR) ? ' ' : 0;
        std::memset(nb2->data(), fill, nb);
    }
    if (heap_->buffer && es > 0) {
        size_t cr = std::min(oR, nr), cc = std::min(oC, nc), cp = std::min(oP, np);
        const char *s = static_cast<const char *>(heap_->buffer->data());
        char *d = static_cast<char *>(nb2->data());
        for (size_t p = 0; p < cp; ++p)
            for (size_t c = 0; c < cc; ++c)
                std::memcpy(d + (p * nr * nc + c * nr) * es,
                            s + (p * oR * oC + c * oR) * es,
                            cr * es);
    }
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = nb2;
    heap_->mr = mr;
    heap_->dims = {nr, nc, np};
    heap_->appendCapacity = 0;
}

void Value::resizeND(const size_t *newDims, int nd, std::pmr::memory_resource *mr)
{
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd <= 0 || nd > kMaxNd)
        throw std::runtime_error("resizeND: rank out of range");
    if (nd == 1) { resize(newDims[0], 1, mr); return; }
    if (nd == 2) { resize(newDims[0], newDims[1], mr); return; }
    if (nd == 3) { resize3d(newDims[0], newDims[1], newDims[2], mr); return; }

    // Promote scalar (no heap_) to a 1×1 DOUBLE array so the rest can
    // assume heap_ is set and detach() is meaningful.
    if (heap_ == nullptr) {
        double v = scalar_;
        auto *h = new HeapObject();
        h->type = ValueType::DOUBLE;
        h->dims = {1, 1};
        h->mr = mr;
        h->buffer = new DataBuffer(sizeof(double), mr);
        *static_cast<double *>(h->buffer->data()) = v;
        heap_ = h;
    }
    // Empty (tag) → fresh DOUBLE heap with the requested shape; no
    // existing data to preserve. Same path is used for the auto-grow
    // case `A(i,j,k,l) = v` when A doesn't exist yet.
    if (heap_ == emptyTag()) {
        *this = matrixND(newDims, nd, ValueType::DOUBLE, mr);
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot resize");
    detach();
    if (!mr) mr = heap_->mr;

    const ValueType t = heap_->type;
    const Dims oldDims = heap_->dims;
    const int oldNd = oldDims.ndim();

    size_t newTotal = 1;
    for (int i = 0; i < nd; ++i) newTotal *= newDims[i];

    size_t oldStrides[kMaxNd], newStrides[kMaxNd];
    computeStridesColMajor(oldDims, oldStrides);
    Dims newD(newDims, nd);
    computeStridesColMajor(newD, newStrides);

    const int commonNd = std::min(nd, oldNd);
    size_t commonDims[kMaxNd];
    size_t commonTotal = 1;
    for (int i = 0; i < commonNd; ++i) {
        commonDims[i] = std::min(oldDims.dim(i), newDims[i]);
        commonTotal *= commonDims[i];
    }

    if (t == ValueType::CELL) {
        auto newCell = Value::cellND(newDims, nd);
        if (commonTotal > 0) {
            Dims commonD(commonDims, commonNd);
            size_t coords[kMaxNd] = {0};
            do {
                size_t oldOff = 0, newOff = 0;
                for (int i = 0; i < commonNd; ++i) {
                    oldOff += coords[i] * oldStrides[i];
                    newOff += coords[i] * newStrides[i];
                }
                newCell.cellAt(newOff) = cellAt(oldOff);
            } while (incrementCoords(coords, commonD));
        }
        *this = std::move(newCell);
        return;
    }

    const size_t es = elementSize(t);
    if (es == 0)
        throw std::runtime_error(
            std::string("resizeND not supported for type '") + mtypeName(t) + "'");

    const size_t nb = newTotal * es;
    auto *nb2 = new DataBuffer(nb, mr);
    if (nb > 0) {
        const int fill = (t == ValueType::CHAR) ? ' ' : 0;
        std::memset(nb2->data(), fill, nb);
    }
    if (heap_->buffer && commonTotal > 0) {
        const char *src = static_cast<const char *>(heap_->buffer->data());
        char *dst = static_cast<char *>(nb2->data());
        Dims commonD(commonDims, commonNd);
        size_t coords[kMaxNd] = {0};
        do {
            size_t oldOff = 0, newOff = 0;
            for (int i = 0; i < commonNd; ++i) {
                oldOff += coords[i] * oldStrides[i];
                newOff += coords[i] * newStrides[i];
            }
            std::memcpy(dst + newOff * es, src + oldOff * es, es);
        } while (incrementCoords(coords, commonD));
    }
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = nb2;
    heap_->mr = mr;
    heap_->dims = newD;
    heap_->appendCapacity = 0;
}

void Value::ensureSize(size_t idx, std::pmr::memory_resource *mr)
{
    if (heap_ == emptyTag() || (heap_ == nullptr && idx > 0)) {
        double old = (heap_ == nullptr) ? scalar_ : 0.0;
        *this = matrix(1, idx + 1, ValueType::DOUBLE, mr);
        if (old != 0.0)
            static_cast<double *>(heap_->buffer->data())[0] = old;
        return;
    }
    size_t need = idx + 1;
    if (need > numel()) {
        bool isColVec = (dims().cols() == 1 && dims().rows() > 1);
        if (isColVec)
            resize(need, 1, mr); // preserve column vector shape
        else if (dims().isVector() || dims().rows() <= 1)
            resize(1, need, mr);
        else
            throw std::runtime_error("Index exceeds array dimensions");
    }
}

void Value::appendScalar(double v, std::pmr::memory_resource *mr)
{
    size_t oldN = numel(), newN = oldN + 1;

    // Empty → fresh 1-element heap double with headroom.
    if (isEmpty()) {
        size_t cap = 8;
        auto *h = new HeapObject();
        h->type = ValueType::DOUBLE;
        h->dims = {1, 1};
        h->mr = mr;
        h->buffer = new DataBuffer(cap * sizeof(double), mr);
        h->appendCapacity = cap;
        double *d = static_cast<double *>(h->buffer->data());
        std::memset(d, 0, cap * sizeof(double));
        d[0] = v;
        heap_ = h;   // emptyTag is a static sentinel, nothing to release
        return;
    }

    // Inline scalar → promote to a 2-element heap double with headroom.
    if (heap_ == nullptr) {
        double old = scalar_;
        size_t cap = std::max(size_t(8), newN * 2);
        auto *h = new HeapObject();
        h->type = ValueType::DOUBLE;
        h->dims = {1, newN};
        h->mr = mr;
        h->buffer = new DataBuffer(cap * sizeof(double), mr);
        h->appendCapacity = cap;
        double *d = static_cast<double *>(h->buffer->data());
        std::memset(d, 0, cap * sizeof(double));
        d[0] = old;
        d[1] = v;
        heap_ = h;
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot append");
    detach();
    if (!mr)
        mr = heap_->mr;
    size_t cap = heap_->appendCapacity;
    if (!cap && heap_->buffer)
        cap = heap_->buffer->bytes() / sizeof(double);
    if (newN <= cap && heap_->buffer) {
        static_cast<double *>(heap_->buffer->data())[oldN] = v;
        heap_->dims = {1, newN};
        return;
    }
    size_t nc = std::max(newN, cap * 2);
    if (nc < 8)
        nc = 8;
    auto *nb = new DataBuffer(nc * sizeof(double), mr);
    double *d = static_cast<double *>(nb->data());
    std::memset(d, 0, nc * sizeof(double));
    if (oldN > 0 && heap_->buffer)
        std::memcpy(d, heap_->buffer->data(), oldN * sizeof(double));
    d[oldN] = v;
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = nb;
    heap_->mr = mr;
    heap_->dims = {1, newN};
    heap_->appendCapacity = nc;
}

Value &Value::cellAt(size_t i)
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    if (i >= heap_->cellData->size())
        throw std::runtime_error("Cell index out of bounds");
    detach(); // COW: ensure we have our own copy before mutation
    return (*heap_->cellData)[i];
}
const Value &Value::cellAt(size_t i) const
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    if (i >= heap_->cellData->size())
        throw std::runtime_error("Cell index out of bounds");
    return (*heap_->cellData)[i];
}
std::pmr::vector<Value> &Value::cellDataVec()
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    detach(); // COW
    return *heap_->cellData;
}
const std::pmr::vector<Value> &Value::cellDataVec() const
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    return *heap_->cellData;
}

Value &Value::field(const std::string &n)
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    detach(); // COW
    return (*heap_->structData)[n];
}
const Value &Value::field(const std::string &n) const
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    auto it = heap_->structData->find(n);
    if (it == heap_->structData->end())
        throw std::runtime_error("Field not found: " + n);
    return it->second;
}
bool Value::hasField(const std::string &n) const
{
    return isHeap() && heap_->structData && heap_->structData->count(n) > 0;
}
std::pmr::map<std::string, Value> &Value::structFields()
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    return *heap_->structData;
}
const std::pmr::map<std::string, Value> &Value::structFields() const
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    return *heap_->structData;
}

std::string Value::debugString() const
{
    std::ostringstream os;
    ValueType t = type();
    os << mtypeName(t) << " [" << dims().rows() << "x" << dims().cols();
    if (dims().is3D())
        os << "x" << dims().pages();
    os << "]";
    if (t == ValueType::DOUBLE && numel() <= 20 && numel() > 0) {
        os << " = ";
        const double *dd = doubleData();
        if (isScalar())
            os << dd[0];
        else {
            os << "[";
            for (size_t i = 0; i < numel(); ++i) {
                if (i)
                    os << ", ";
                os << dd[i];
            }
            os << "]";
        }
    }
    if (t == ValueType::COMPLEX && numel() <= 20 && numel() > 0 && isHeap() && heap_->buffer) {
        os << " = ";
        const Complex *cd = complexData();
        if (isScalar()) {
            os << cd[0].real();
            if (cd[0].imag() >= 0)
                os << "+";
            os << cd[0].imag() << "i";
        } else {
            os << "[";
            for (size_t i = 0; i < numel(); ++i) {
                if (i)
                    os << ", ";
                os << cd[i].real();
                if (cd[i].imag() >= 0)
                    os << "+";
                os << cd[i].imag() << "i";
            }
            os << "]";
        }
    }
    if (t == ValueType::LOGICAL && numel() > 0) {
        os << " = ";
        if (isScalar())
            os << (toBool() ? "true" : "false");
        else if (isHeap() && heap_->buffer) {
            const uint8_t *ld = logicalData();
            os << "[";
            for (size_t i = 0; i < numel(); ++i) {
                if (i)
                    os << ", ";
                os << (ld[i] ? "1" : "0");
            }
            os << "]";
        }
    }
    if (t == ValueType::CHAR && isHeap() && heap_->buffer)
        os << " = '" << toString() << "'";
    if (t == ValueType::STRING) {
        if (isScalar())
            os << " = \"" << toString() << "\"";
        else if (isHeap() && heap_->cellData) {
            os << " [";
            for (size_t i = 0; i < heap_->cellData->size() && i < 10; ++i) {
                if (i)
                    os << ", ";
                os << "\"" << (*heap_->cellData)[i].toString() << "\"";
            }
            os << "]";
        }
    }
    if (t == ValueType::FUNC_HANDLE)
        os << " = @" << funcHandleName();
    if (t == ValueType::CELL && isHeap() && heap_->cellData) {
        os << " {";
        for (size_t i = 0; i < heap_->cellData->size() && i < 10; ++i) {
            if (i)
                os << ", ";
            os << (*heap_->cellData)[i].debugString();
        }
        if (heap_->cellData->size() > 10)
            os << ", ...";
        os << "}";
    }
    return os.str();
}

// ============================================================
// MATLAB-style display formatting
// ============================================================

// Format a double the way MATLAB does: integers without decimal point,
// short format for floats, aligned columns for matrices.
static std::string fmtDouble(double v)
{
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "Inf" : "-Inf";
    if (v == 0.0) return "0";
    // Integer check
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        std::ostringstream os;
        os << static_cast<int64_t>(v);
        return os.str();
    }
    std::ostringstream os;
    os << v;
    return os.str();
}

static std::string fmtComplex(const Complex &c)
{
    std::ostringstream os;
    if (c.real() != 0.0 || c.imag() == 0.0)
        os << fmtDouble(c.real());
    if (c.imag() != 0.0) {
        if (c.real() != 0.0 && c.imag() > 0) os << " + ";
        else if (c.real() != 0.0 && c.imag() < 0) os << " - ";
        double ai = std::abs(c.imag());
        if (ai == 1.0)
            os << "i";
        else
            os << fmtDouble(ai) << "i";
    }
    return os.str();
}

std::string Value::formatDisplay(const std::string &name) const
{
    std::ostringstream os;

    // Header: "name =\n" (skip for "ans" — MATLAB prints "ans =\n" too, actually)
    if (!name.empty())
        os << name << " =\n";

    ValueType t = type();

    switch (t) {
    case ValueType::DOUBLE: {
        if (isScalar()) {
            os << "   " << fmtDouble(toScalar()) << "\n";
        } else if (isEmpty()) {
            os << "     []\n";
        } else {
            auto &d = dims();
            const size_t R = d.rows(), C = d.cols();
            const int nd = d.ndim();
            const double *base0 = doubleData();
            forEachOuterPage(d, [&](size_t plin, const size_t *outerCoords) {
                if (nd >= 3) {
                    os << "\n(:,:";
                    for (int i = 2; i < nd; ++i)
                        os << "," << outerCoords[i - 2] + 1;
                    os << ") =\n\n";
                }
                const double *page = base0 + plin * R * C;
                std::vector<std::vector<std::string>> cells(R);
                std::vector<size_t> colWidth(C, 0);
                for (size_t r = 0; r < R; ++r) {
                    cells[r].resize(C);
                    for (size_t c = 0; c < C; ++c) {
                        cells[r][c] = fmtDouble(page[c * R + r]);
                        colWidth[c] = std::max(colWidth[c], cells[r][c].size());
                    }
                }
                for (size_t r = 0; r < R; ++r) {
                    os << "   ";
                    for (size_t c = 0; c < C; ++c) {
                        size_t pad = colWidth[c] - cells[r][c].size();
                        for (size_t i = 0; i < pad + 1; ++i) os << ' ';
                        os << cells[r][c];
                    }
                    os << "\n";
                }
            });
        }
        break;
    }
    case ValueType::CHAR: {
        // Row (or 1×0) stays on a single quoted line, as before.
        // Multi-row char matrix prints each row quoted on its own
        // line, column-major — MATLAB's implicit-display layout.
        auto &d = dims();
        if (d.rows() <= 1) {
            os << "   '" << toString() << "'\n";
        } else {
            for (size_t r = 0; r < d.rows(); ++r)
                os << "   '" << charRow(r) << "'\n";
        }
        break;
    }
    case ValueType::STRING:
        if (isScalar()) {
            os << "   \"" << toString() << "\"\n";
        } else {
            auto &d = dims();
            os << "  " << d.rows() << "x" << d.cols() << " string array\n";
            for (size_t i = 0; i < numel() && i < 20; ++i)
                os << "    \"" << stringElem(i) << "\"\n";
        }
        break;
    case ValueType::LOGICAL: {
        if (isScalar()) {
            os << "   " << (toBool() ? "1" : "0") << "\n";
        } else {
            auto &d = dims();
            for (size_t p = 0; p < d.pages(); ++p) {
                if (d.is3D())
                    os << "\n(:,:," << p + 1 << ") =\n\n";
                for (size_t r = 0; r < d.rows(); ++r) {
                    os << "   ";
                    for (size_t c = 0; c < d.cols(); ++c) {
                        size_t idx = d.is3D() ? (p * d.rows() * d.cols() + c * d.rows() + r)
                                              : d.sub2ind(r, c);
                        os << " " << (logicalData()[idx] ? "1" : "0");
                    }
                    os << "\n";
                }
            }
        }
        break;
    }
    case ValueType::COMPLEX: {
        if (isScalar()) {
            os << "   " << fmtComplex(toComplex()) << "\n";
        } else {
            auto &d = dims();
            // Pre-format for alignment
            std::vector<std::vector<std::string>> cells(d.rows());
            std::vector<size_t> colWidth(d.cols(), 0);
            for (size_t r = 0; r < d.rows(); ++r) {
                cells[r].resize(d.cols());
                for (size_t c = 0; c < d.cols(); ++c) {
                    cells[r][c] = fmtComplex(complexElem(r, c));
                    colWidth[c] = std::max(colWidth[c], cells[r][c].size());
                }
            }
            for (size_t r = 0; r < d.rows(); ++r) {
                os << "   ";
                for (size_t c = 0; c < d.cols(); ++c) {
                    size_t pad = colWidth[c] - cells[r][c].size();
                    for (size_t i = 0; i < pad + 1; ++i) os << ' ';
                    os << cells[r][c];
                }
                os << "\n";
            }
        }
        break;
    }
    case ValueType::SINGLE: {
        if (isScalar()) {
            os << "   " << static_cast<double>(*singleData()) << "\n";
        } else if (isEmpty()) {
            os << "     []\n";
        } else {
            auto &d = dims();
            const float *fd = singleData();
            for (size_t p = 0; p < d.pages(); ++p) {
                if (d.is3D())
                    os << "\n(:,:," << p + 1 << ") =\n\n";
                for (size_t r = 0; r < d.rows(); ++r) {
                    os << "   ";
                    for (size_t c = 0; c < d.cols(); ++c) {
                        size_t idx = d.is3D() ? d.sub2ind(r, c, p) : d.sub2ind(r, c);
                        os << " " << static_cast<double>(fd[idx]);
                    }
                    os << "\n";
                }
            }
        }
        break;
    }
    case ValueType::INT8:
    case ValueType::INT16:
    case ValueType::INT32:
    case ValueType::INT64:
    case ValueType::UINT8:
    case ValueType::UINT16:
    case ValueType::UINT32:
    case ValueType::UINT64: {
        if (isScalar()) {
            os << "   " << static_cast<int64_t>(toScalar()) << "\n";
        } else if (isEmpty()) {
            os << "     []\n";
        } else {
            auto &d = dims();
            for (size_t p = 0; p < d.pages(); ++p) {
                if (d.is3D())
                    os << "\n(:,:," << p + 1 << ") =\n\n";
                for (size_t r = 0; r < d.rows(); ++r) {
                    os << "   ";
                    for (size_t c = 0; c < d.cols(); ++c) {
                        size_t idx = d.is3D() ? d.sub2ind(r, c, p) : d.sub2ind(r, c);
                        const void *raw = rawData();
                        int64_t val = 0;
                        switch (t) {
                        case ValueType::INT8:   val = static_cast<const int8_t*>(raw)[idx]; break;
                        case ValueType::INT16:  val = static_cast<const int16_t*>(raw)[idx]; break;
                        case ValueType::INT32:  val = static_cast<const int32_t*>(raw)[idx]; break;
                        case ValueType::INT64:  val = static_cast<const int64_t*>(raw)[idx]; break;
                        case ValueType::UINT8:  val = static_cast<const uint8_t*>(raw)[idx]; break;
                        case ValueType::UINT16: val = static_cast<const uint16_t*>(raw)[idx]; break;
                        case ValueType::UINT32: val = static_cast<const uint32_t*>(raw)[idx]; break;
                        case ValueType::UINT64: val = static_cast<int64_t>(static_cast<const uint64_t*>(raw)[idx]); break;
                        default: break;
                        }
                        os << " " << val;
                    }
                    os << "\n";
                }
            }
        }
        break;
    }
    case ValueType::STRUCT:
        os << "  struct with fields:\n\n";
        for (auto &[k, v] : structFields())
            os << "    " << k << ": " << v.debugString() << "\n";
        break;
    case ValueType::FUNC_HANDLE:
        os << "   @" << funcHandleName() << "\n";
        break;
    case ValueType::CELL: {
        auto &d = dims();
        os << "  {" << d.rows() << "x" << d.cols();
        if (d.is3D()) os << "x" << d.pages();
        os << " cell}\n";
        for (size_t i = 0; i < numel() && i < 20; ++i)
            os << "    {" << i + 1 << "}: " << cellAt(i).debugString() << "\n";
        if (numel() > 20)
            os << "    ... (" << numel() - 20 << " more)\n";
        break;
    }
    case ValueType::EMPTY:
        os << "     []\n";
        break;
    default:
        os << "   " << debugString() << "\n";
        break;
    }

    os << "\n";
    return os.str();
}

} // namespace numkit