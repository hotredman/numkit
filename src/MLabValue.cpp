#include "MLabValue.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace mlab {

const char *mtypeName(MType t)
{
    switch (t) {
    case MType::EMPTY:
        return "empty";
    case MType::DOUBLE:
        return "double";
    case MType::COMPLEX:
        return "complex";
    case MType::LOGICAL:
        return "logical";
    case MType::CHAR:
        return "char";
    case MType::CELL:
        return "cell";
    case MType::STRUCT:
        return "struct";
    case MType::FUNC_HANDLE:
        return "function_handle";
    case MType::INT8:
        return "int8";
    case MType::INT16:
        return "int16";
    case MType::INT32:
        return "int32";
    case MType::INT64:
        return "int64";
    case MType::UINT8:
        return "uint8";
    case MType::UINT16:
        return "uint16";
    case MType::UINT32:
        return "uint32";
    case MType::UINT64:
        return "uint64";
    }
    return "unknown";
}
size_t elementSize(MType t)
{
    switch (t) {
    case MType::DOUBLE:
        return 8;
    case MType::COMPLEX:
        return 16;
    case MType::LOGICAL:
        return 1;
    case MType::CHAR:
        return 1;
    case MType::INT8:
        return 1;
    case MType::INT16:
        return 2;
    case MType::INT32:
        return 4;
    case MType::INT64:
        return 8;
    case MType::UINT8:
        return 1;
    case MType::UINT16:
        return 2;
    case MType::UINT32:
        return 4;
    case MType::UINT64:
        return 8;
    default:
        return 0;
    }
}

Dims::Dims()
{
    d[0] = 0;
    d[1] = 0;
    d[2] = 1;
    nd = 2;
}
Dims::Dims(size_t r, size_t c)
{
    d[0] = r;
    d[1] = c;
    d[2] = 1;
    nd = 2;
}
Dims::Dims(size_t r, size_t c, size_t p)
{
    d[0] = r;
    d[1] = c;
    d[2] = p;
    nd = 3;
}
int Dims::ndims() const
{
    return nd;
}
size_t Dims::rows() const
{
    return d[0];
}
size_t Dims::cols() const
{
    return d[1];
}
size_t Dims::pages() const
{
    return d[2];
}
size_t Dims::numel() const
{
    return d[0] * d[1] * d[2];
}
bool Dims::isScalar() const
{
    return numel() == 1;
}
bool Dims::isEmpty() const
{
    return numel() == 0;
}
bool Dims::isVector() const
{
    return nd == 2 && (d[0] == 1 || d[1] == 1);
}
bool Dims::is3D() const
{
    return nd == 3 && d[2] > 1;
}
size_t Dims::dimSize(int dim) const
{
    return (dim >= 0 && dim < 3) ? d[dim] : 1;
}
size_t Dims::sub2ind(size_t r, size_t c) const
{
    return c * d[0] + r;
}
size_t Dims::sub2ind(size_t r, size_t c, size_t p) const
{
    return p * d[0] * d[1] + c * d[0] + r;
}
size_t Dims::sub2indChecked(size_t r, size_t c) const
{
    if (r >= d[0] || c >= d[1])
        throw std::runtime_error("Index (" + std::to_string(r + 1) + "," + std::to_string(c + 1)
                                 + ") exceeds dimensions");
    return c * d[0] + r;
}
size_t Dims::sub2indChecked(size_t r, size_t c, size_t p) const
{
    if (r >= d[0] || c >= d[1] || p >= d[2])
        throw std::runtime_error("Index exceeds dimensions");
    return p * d[0] * d[1] + c * d[0] + r;
}
bool Dims::operator==(const Dims &o) const
{
    return d[0] == o.d[0] && d[1] == o.d[1] && d[2] == o.d[2];
}
bool Dims::operator!=(const Dims &o) const
{
    return !(*this == o);
}

DataBuffer::DataBuffer(size_t bytes, Allocator *alloc)
    : bytes_(bytes)
    , refCount_(1)
    , allocator_(alloc)
{
    if (bytes > 0) {
        if (alloc && alloc->allocate)
            data_ = alloc->allocate(bytes);
        else
            data_ = ::operator new(bytes);
        if (!data_)
            throw std::runtime_error("DataBuffer: allocation failed");
    }
}
DataBuffer::~DataBuffer()
{
    if (data_) {
        if (allocator_ && allocator_->deallocate)
            allocator_->deallocate(data_, bytes_);
        else
            ::operator delete(data_);
    }
}
void DataBuffer::addRef()
{
    refCount_.fetch_add(1, std::memory_order_relaxed);
}
bool DataBuffer::release()
{
    return refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1;
}
int DataBuffer::refCount() const
{
    return refCount_.load(std::memory_order_acquire);
}

HeapObject::~HeapObject()
{
    if (buffer) {
        if (buffer->release())
            delete buffer;
    }
    delete cellData;
    delete structData;
    delete funcName;
}
HeapObject *HeapObject::clone() const
{
    auto *h = new HeapObject();
    h->type = type;
    h->dims = dims;
    h->allocator = allocator;
    if (buffer) {
        h->buffer = new DataBuffer(buffer->bytes(), allocator);
        std::memcpy(h->buffer->data(), buffer->data(), buffer->bytes());
    }
    if (cellData)
        h->cellData = new std::vector<MValue>(*cellData);
    if (structData)
        h->structData = new std::map<std::string, MValue>(*structData);
    if (funcName)
        h->funcName = new std::string(*funcName);
    return h;
}

HeapObject MValue::sEmptyTag;
HeapObject MValue::sLogicalTrue;
HeapObject MValue::sLogicalFalse;
const Dims MValue::sScalarDims{1, 1};
const Dims MValue::sEmptyDims{};

MValue::MValue()
    : scalar_(0.0)
    , heap_(emptyTag())
{}
MValue::~MValue()
{
    releaseHeap();
}
MValue::MValue(const MValue &o)
    : scalar_(o.scalar_)
    , heap_(o.heap_)
{
    if (o.isHeap())
        heap_->addRef();
}
MValue &MValue::operator=(const MValue &o)
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
MValue::MValue(MValue &&o) noexcept
    : scalar_(o.scalar_)
    , heap_(o.heap_)
{
    o.heap_ = emptyTag();
    o.scalar_ = 0.0;
}
MValue &MValue::operator=(MValue &&o) noexcept
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
void MValue::swap(MValue &o) noexcept
{
    std::swap(scalar_, o.scalar_);
    std::swap(heap_, o.heap_);
}
void MValue::releaseHeap()
{
    if (isHeap()) {
        if (heap_->release())
            delete heap_;
    }
    heap_ = emptyTag();
}
void MValue::detach()
{
    if (!isHeap())
        return;
    if (heap_->refCount.load(std::memory_order_acquire) <= 1)
        return;
    HeapObject *c = heap_->clone();
    releaseHeap();
    heap_ = c;
}
HeapObject *MValue::mutableHeap()
{
    detach();
    return heap_;
}

MValue MValue::scalar(double v, Allocator *)
{
    MValue m;
    m.scalar_ = v;
    m.heap_ = nullptr;
    return m;
}
MValue MValue::logicalScalar(bool v, Allocator *)
{
    MValue m;
    m.heap_ = v ? logicalTrueTag() : logicalFalseTag();
    return m;
}
MValue MValue::empty()
{
    return MValue();
}

MValue MValue::matrix(size_t rows, size_t cols, MType t, Allocator *alloc)
{
    if (rows == 1 && cols == 1 && t == MType::DOUBLE)
        return scalar(0.0, alloc);
    MValue m;
    auto *h = new HeapObject();
    h->type = t;
    h->dims = {rows, cols};
    h->allocator = alloc;
    size_t bytes = rows * cols * elementSize(t);
    if (bytes > 0) {
        h->buffer = new DataBuffer(bytes, alloc);
        std::memset(h->buffer->data(), 0, bytes);
    }
    m.heap_ = h;
    return m;
}
MValue MValue::matrix3d(size_t rows, size_t cols, size_t pages, MType t, Allocator *alloc)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = t;
    h->dims = {rows, cols, pages};
    h->allocator = alloc;
    size_t bytes = rows * cols * pages * elementSize(t);
    if (bytes > 0) {
        h->buffer = new DataBuffer(bytes, alloc);
        std::memset(h->buffer->data(), 0, bytes);
    }
    m.heap_ = h;
    return m;
}
MValue MValue::fromString(const std::string &s, Allocator *alloc)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::CHAR;
    h->dims = {1, s.size()};
    h->allocator = alloc;
    if (!s.empty()) {
        h->buffer = new DataBuffer(s.size(), alloc);
        std::memcpy(h->buffer->data(), s.data(), s.size());
    }
    m.heap_ = h;
    return m;
}
MValue MValue::cell(size_t rows, size_t cols)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::CELL;
    h->dims = {rows, cols};
    h->cellData = new std::vector<MValue>(rows * cols);
    m.heap_ = h;
    return m;
}
MValue MValue::cell3D(size_t rows, size_t cols, size_t pages)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::CELL;
    h->dims = {rows, cols, pages};
    h->cellData = new std::vector<MValue>(rows * cols * pages);
    m.heap_ = h;
    return m;
}
MValue MValue::structure()
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::STRUCT;
    h->dims = {1, 1};
    h->structData = new std::map<std::string, MValue>();
    m.heap_ = h;
    return m;
}
MValue MValue::funcHandle(const std::string &name, Allocator *alloc)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::FUNC_HANDLE;
    h->dims = {1, 1};
    h->allocator = alloc;
    h->funcName = new std::string(name);
    m.heap_ = h;
    return m;
}
// ============================================================
// Colon range: start:step:stop → row vector
// ============================================================
static size_t colonCount(double start, double step, double stop)
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
    return static_cast<size_t>(n);
}

MValue MValue::colonRange(double start, double stop, Allocator *alloc)
{
    return colonRange(start, 1.0, stop, alloc);
}

MValue MValue::colonRange(double start, double step, double stop, Allocator *alloc)
{
    size_t count = colonCount(start, step, stop);
    auto result = MValue::matrix(1, count, MType::DOUBLE, alloc);
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
static MType promoteNumericType(const MValue *elems, size_t count)
{
    bool hasDouble = false;
    bool hasComplex = false;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        switch (elems[i].type()) {
        case MType::COMPLEX:
            hasComplex = true;
            break;
        case MType::DOUBLE:
            hasDouble = true;
            break;
        case MType::LOGICAL:
            break;
        default:
            throw std::runtime_error(
                std::string("Concatenation not supported for type '")
                + mtypeName(elems[i].type()) + "'");
        }
    }
    if (hasComplex)
        return MType::COMPLEX;
    if (hasDouble)
        return MType::DOUBLE;
    return MType::LOGICAL; // all-logical stays logical
}

// Read one element as double. Supports DOUBLE, LOGICAL, COMPLEX (takes real part).
static double readElemAsDouble(const MValue &v, size_t idx)
{
    switch (v.type()) {
    case MType::DOUBLE:
        return v.doubleData()[idx];
    case MType::LOGICAL:
        return static_cast<double>(v.logicalData()[idx]);
    case MType::COMPLEX:
        return v.complexData()[idx].real();
    default:
        throw std::runtime_error(
            std::string("Cannot read element as double from type '")
            + mtypeName(v.type()) + "'");
    }
}

// Read one element as Complex. Supports DOUBLE, LOGICAL, COMPLEX.
static Complex readElemAsComplex(const MValue &v, size_t idx)
{
    switch (v.type()) {
    case MType::COMPLEX:
        return v.complexData()[idx];
    case MType::DOUBLE:
        return Complex(v.doubleData()[idx], 0.0);
    case MType::LOGICAL:
        return Complex(static_cast<double>(v.logicalData()[idx]), 0.0);
    default:
        throw std::runtime_error(
            std::string("Cannot read element as complex from type '")
            + mtypeName(v.type()) + "'");
    }
}

// Read one element as uint8_t logical. Supports LOGICAL, DOUBLE.
static uint8_t readElemAsLogical(const MValue &v, size_t idx)
{
    switch (v.type()) {
    case MType::LOGICAL:
        return v.logicalData()[idx];
    case MType::DOUBLE:
        return v.doubleData()[idx] != 0.0 ? 1 : 0;
    default:
        throw std::runtime_error(
            std::string("Cannot read element as logical from type '")
            + mtypeName(v.type()) + "'");
    }
}

// Get dimensions for a concat element: rows, cols, pages.
// Scalars are treated as 1×1×1.
static void getElemDims(const MValue &v, size_t &r, size_t &c, size_t &p)
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
                      const MValue &src, size_t srcRows, size_t srcCols, size_t srcPages,
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
MValue MValue::horzcat(const MValue *elems, size_t count, Allocator *alloc)
{
    // String concatenation path
    bool hasChar = false;
    for (size_t i = 0; i < count; ++i)
        if (!elems[i].isEmpty() && elems[i].type() == MType::CHAR) {
            hasChar = true;
            break;
        }

    if (hasChar) {
        std::string result;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].type() == MType::CHAR)
                result += elems[i].toString();
            else if (elems[i].isScalar())
                result += static_cast<char>(static_cast<int>(elems[i].toScalar()));
            else
                throw std::runtime_error("Cannot concatenate char and non-char arrays");
        }
        return MValue::fromString(result, alloc);
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
    if (!rows)
        rows = 1;

    MType outType = promoteNumericType(elems, count);

    auto result = (pages > 1) ? MValue::matrix3d(rows, totalCols, pages, outType, alloc)
                              : MValue::matrix(rows, totalCols, outType, alloc);

    size_t colOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);

        if (outType == MType::COMPLEX)
            copyBlock(result.complexDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, readElemAsComplex);
        else if (outType == MType::LOGICAL)
            copyBlock(result.logicalDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, readElemAsLogical);
        else
            copyBlock(result.doubleDataMut(), rows, totalCols,
                      elems[i], eR, eC, eP, 0, colOff, pages, readElemAsDouble);
        colOff += eC;
    }
    return result;
}

// ============================================================
// Vertical concatenation: [a; b; c]
// Concatenates along dimension 1 (rows).
// Columns and pages must match across all elements.
// ============================================================
MValue MValue::vertcat(const MValue *elems, size_t count, Allocator *alloc)
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
        return MValue::empty();

    // Cell vertcat: combine rows into a 2D cell (column-major)
    if (hasCell) {
        auto result = MValue::cell(totalRows, cols);
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

    MType outType = promoteNumericType(elems, count);

    auto result = (pages > 1) ? MValue::matrix3d(totalRows, cols, pages, outType, alloc)
                              : MValue::matrix(totalRows, cols, outType, alloc);

    size_t rowOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eR, eC, eP;
        getElemDims(elems[i], eR, eC, eP);

        if (outType == MType::COMPLEX)
            copyBlock(result.complexDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, readElemAsComplex);
        else if (outType == MType::LOGICAL)
            copyBlock(result.logicalDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, readElemAsLogical);
        else
            copyBlock(result.doubleDataMut(), totalRows, cols,
                      elems[i], eR, eC, eP, rowOff, 0, pages, readElemAsDouble);
        rowOff += eR;
    }
    return result;
}

// ============================================================
// Type-preserving indexing
// ============================================================

// Helper: create a scalar MValue of the same type as this array at linear index.
MValue MValue::elemAt(size_t idx, Allocator *alloc) const
{
    switch (type()) {
    case MType::DOUBLE:
        return MValue::scalar(doubleData()[idx], alloc);
    case MType::COMPLEX:
        return MValue::complexScalar(complexData()[idx], alloc);
    case MType::LOGICAL:
        return MValue::logicalScalar(logicalData()[idx] != 0, alloc);
    case MType::CHAR: {
        std::string s(1, charData()[idx]);
        return MValue::fromString(s, alloc);
    }
    case MType::CELL:
        return cellAt(idx);
    default:
        throw std::runtime_error(
            std::string("elemAt not supported for type '") + mtypeName(type()) + "'");
    }
}

// 1D slice: extract elements at given linear indices → row vector of same type.
MValue MValue::indexGet(const size_t *indices, size_t count, Allocator *alloc) const
{
    if (count == 1)
        return elemAt(indices[0], alloc);

    MType t = type();
    switch (t) {
    case MType::DOUBLE: {
        auto result = MValue::matrix(1, count, MType::DOUBLE, alloc);
        double *dst = result.doubleDataMut();
        const double *src = doubleData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case MType::COMPLEX: {
        auto result = MValue::complexMatrix(1, count, alloc);
        Complex *dst = result.complexDataMut();
        const Complex *src = complexData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case MType::LOGICAL: {
        auto result = MValue::matrix(1, count, MType::LOGICAL, alloc);
        uint8_t *dst = result.logicalDataMut();
        const uint8_t *src = logicalData();
        for (size_t i = 0; i < count; ++i)
            dst[i] = src[indices[i]];
        return result;
    }
    case MType::CHAR: {
        std::string s;
        s.reserve(count);
        const char *src = charData();
        for (size_t i = 0; i < count; ++i)
            s += src[indices[i]];
        return MValue::fromString(s, alloc);
    }
    case MType::CELL: {
        auto result = MValue::cell(1, count);
        for (size_t i = 0; i < count; ++i)
            result.cellAt(i) = cellAt(indices[i]);
        return result;
    }
    default:
        throw std::runtime_error(
            std::string("indexGet not supported for type '") + mtypeName(t) + "'");
    }
}

// 2D slice: extract sub-matrix at given row/col indices.
MValue MValue::indexGet2D(const size_t *rowIdx, size_t nrows,
                          const size_t *colIdx, size_t ncols,
                          Allocator *alloc) const
{
    if (nrows == 1 && ncols == 1) {
        size_t idx = dims().sub2ind(rowIdx[0], colIdx[0]);
        return elemAt(idx, alloc);
    }

    MType t = type();

    if (t == MType::CELL) {
        auto &d = dims();
        auto result = MValue::cell(nrows, ncols);
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
    auto result = MValue::matrix(nrows, ncols, t, alloc);
    const char *src = static_cast<const char *>(rawData());
    char *dst = static_cast<char *>(result.rawDataMut());
    for (size_t c = 0; c < ncols; ++c)
        for (size_t r = 0; r < nrows; ++r)
            std::memcpy(dst + (c * nrows + r) * es,
                        src + d.sub2ind(rowIdx[r], colIdx[c]) * es, es);
    return result;
}

// 3D slice: extract sub-array at given row/col/page indices.
MValue MValue::indexGet3D(const size_t *rowIdx, size_t nrows,
                          const size_t *colIdx, size_t ncols,
                          const size_t *pageIdx, size_t npages,
                          Allocator *alloc) const
{
    if (nrows == 1 && ncols == 1 && npages == 1) {
        size_t idx = dims().sub2ind(rowIdx[0], colIdx[0], pageIdx[0]);
        return elemAt(idx, alloc);
    }

    MType t = type();

    if (t == MType::CELL) {
        auto &d = dims();
        auto result = MValue::cell3D(nrows, ncols, npages);
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
    auto result = MValue::matrix3d(nrows, ncols, npages, t, alloc);
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

// Logical indexing: extract elements where mask is true → row vector of same type.
MValue MValue::logicalIndex(const uint8_t *mask, size_t maskLen, Allocator *alloc) const
{
    size_t n = std::min(maskLen, numel());
    // Count selected elements
    size_t selected = 0;
    for (size_t i = 0; i < n; ++i)
        if (mask[i])
            selected++;

    MType t = type();
    switch (t) {
    case MType::DOUBLE: {
        auto result = MValue::matrix(1, selected, MType::DOUBLE, alloc);
        double *dst = result.doubleDataMut();
        const double *src = doubleData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case MType::COMPLEX: {
        auto result = MValue::complexMatrix(1, selected, alloc);
        Complex *dst = result.complexDataMut();
        const Complex *src = complexData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case MType::LOGICAL: {
        auto result = MValue::matrix(1, selected, MType::LOGICAL, alloc);
        uint8_t *dst = result.logicalDataMut();
        const uint8_t *src = logicalData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case MType::CHAR: {
        auto result = MValue::matrix(1, selected, MType::CHAR, alloc);
        char *dst = result.charDataMut();
        const char *src = charData();
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                dst[k++] = src[i];
        return result;
    }
    case MType::CELL: {
        auto result = MValue::cell(1, selected);
        size_t k = 0;
        for (size_t i = 0; i < n; ++i)
            if (mask[i])
                result.cellAt(k++) = cellAt(i);
        return result;
    }
    default:
        throw std::runtime_error(
            std::string("logicalIndex not supported for type '") + mtypeName(t) + "'");
    }
}

// ============================================================
// Index resolution — convert MValue index to vector<size_t> (0-based)
// ============================================================

std::vector<size_t> MValue::resolveIndices(const MValue &idx, size_t dimSize)
{
    std::vector<size_t> out;
    if (idx.isChar() && idx.numel() == 1 && idx.charData()[0] == ':') {
        out.resize(dimSize);
        for (size_t i = 0; i < dimSize; ++i)
            out[i] = i;
    } else if (idx.isLogical()) {
        const uint8_t *m = idx.logicalData();
        for (size_t i = 0; i < idx.numel() && i < dimSize; ++i)
            if (m[i])
                out.push_back(i);
    } else if (idx.isDoubleScalar()) {
        size_t ii = static_cast<size_t>(idx.toScalar()) - 1;
        if (ii >= dimSize)
            throw std::runtime_error("Index exceeds array dimensions");
        out.push_back(ii);
    } else {
        const double *d = idx.doubleData();
        for (size_t i = 0; i < idx.numel(); ++i) {
            size_t ii = static_cast<size_t>(d[i]) - 1;
            if (ii >= dimSize)
                throw std::runtime_error("Index exceeds array dimensions");
            out.push_back(ii);
        }
    }
    return out;
}

std::vector<size_t> MValue::resolveIndicesUnchecked(const MValue &idx)
{
    std::vector<size_t> out;
    if (idx.isLogical()) {
        const uint8_t *m = idx.logicalData();
        for (size_t i = 0; i < idx.numel(); ++i)
            if (m[i])
                out.push_back(i);
    } else if (idx.isDoubleScalar() || idx.isLogicalScalar()) {
        out.push_back(static_cast<size_t>(idx.toScalar()) - 1);
    } else {
        const double *d = idx.doubleData();
        for (size_t i = 0; i < idx.numel(); ++i)
            out.push_back(static_cast<size_t>(d[i]) - 1);
    }
    return out;
}

// ============================================================
// Type-preserving indexed assignment
// ============================================================

// Helper: write one element from val into dst array at linear index.
// Converts val to the destination type. COW detach handled by *DataMut().
static void writeElem(MValue &dst, size_t idx, const MValue &val, size_t valIdx)
{
    switch (dst.type()) {
    case MType::DOUBLE:
        dst.doubleDataMut()[idx] = readElemAsDouble(val, valIdx);
        break;
    case MType::COMPLEX:
        dst.complexDataMut()[idx] = readElemAsComplex(val, valIdx);
        break;
    case MType::LOGICAL:
        dst.logicalDataMut()[idx] = static_cast<uint8_t>(val.toBool());
        break;
    case MType::CHAR:
        dst.charDataMut()[idx] = static_cast<char>(static_cast<int>(readElemAsDouble(val, valIdx)));
        break;
    case MType::CELL:
        dst.cellAt(idx) = val.cellAt(valIdx);
        break;
    default:
        throw std::runtime_error(
            std::string("Indexed assignment not supported for type '")
            + mtypeName(dst.type()) + "'");
    }
}

// Write a scalar val (broadcast) into dst at linear index.
static void writeScalar(MValue &dst, size_t idx, const MValue &val)
{
    switch (dst.type()) {
    case MType::DOUBLE:
        dst.doubleDataMut()[idx] = val.toScalar();
        break;
    case MType::COMPLEX:
        dst.complexDataMut()[idx] = val.toComplex();
        break;
    case MType::LOGICAL:
        dst.logicalDataMut()[idx] = static_cast<uint8_t>(val.toBool());
        break;
    case MType::CHAR:
        dst.charDataMut()[idx] = static_cast<char>(static_cast<int>(val.toScalar()));
        break;
    case MType::CELL:
        dst.cellAt(idx) = val;
        break;
    default:
        throw std::runtime_error(
            std::string("Indexed assignment not supported for type '")
            + mtypeName(dst.type()) + "'");
    }
}

void MValue::elemSet(size_t idx, const MValue &val)
{
    writeScalar(*this, idx, val);
}

void MValue::indexSet(const size_t *indices, size_t count, const MValue &val)
{
    if (val.isScalar()) {
        for (size_t k = 0; k < count; ++k)
            writeScalar(*this, indices[k], val);
    } else {
        size_t n = std::min(count, val.numel());
        for (size_t k = 0; k < n; ++k)
            writeElem(*this, indices[k], val, k);
    }
}

void MValue::indexSet2D(const size_t *rowIdx, size_t nrows,
                        const size_t *colIdx, size_t ncols,
                        const MValue &val)
{
    auto &d = dims();
    if (val.isScalar()) {
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                writeScalar(*this, d.sub2ind(rowIdx[r], colIdx[c]), val);
    } else {
        size_t k = 0, n = val.numel();
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < nrows; ++r)
                if (k < n)
                    writeElem(*this, d.sub2ind(rowIdx[r], colIdx[c]), val, k++);
    }
}

void MValue::indexSet3D(const size_t *rowIdx, size_t nrows,
                        const size_t *colIdx, size_t ncols,
                        const size_t *pageIdx, size_t npages,
                        const MValue &val)
{
    auto &d = dims();
    if (val.isScalar()) {
        for (size_t p = 0; p < npages; ++p)
            for (size_t c = 0; c < ncols; ++c)
                for (size_t r = 0; r < nrows; ++r)
                    writeScalar(*this, d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]), val);
    } else {
        size_t k = 0, n = val.numel();
        for (size_t p = 0; p < npages; ++p)
            for (size_t c = 0; c < ncols; ++c)
                for (size_t r = 0; r < nrows; ++r)
                    if (k < n)
                        writeElem(*this, d.sub2ind(rowIdx[r], colIdx[c], pageIdx[p]), val, k++);
    }
}

// ============================================================
// Type-preserving index deletion
// ============================================================

void MValue::indexDelete(const size_t *indices, size_t count, Allocator *alloc)
{
    MType t = type();
    if (t == MType::STRUCT || t == MType::FUNC_HANDLE || t == MType::EMPTY)
        throw std::runtime_error(
            std::string("Delete indexing not supported for type '") + mtypeName(t) + "'");

    size_t n = numel();
    std::vector<bool> del(n, false);
    for (size_t k = 0; k < count; ++k)
        if (indices[k] < n)
            del[indices[k]] = true;

    size_t remaining = std::count(del.begin(), del.end(), false);
    bool isRow = dims().rows() == 1;

    if (t == MType::CELL) {
        auto &src = cellDataVec();
        auto result = isRow ? MValue::cell(1, remaining) : MValue::cell(remaining, 1);
        auto &dst = result.cellDataVec();
        size_t j = 0;
        for (size_t i = 0; i < n; ++i)
            if (!del[i])
                dst[j++] = src[i];
        *this = std::move(result);
        return;
    }

    size_t es = elementSize(t);
    auto result = isRow ? MValue::matrix(1, remaining, t, alloc)
                        : MValue::matrix(remaining, 1, t, alloc);
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

void MValue::indexDelete2D(const size_t *rowIdx, size_t nrows,
                           const size_t *colIdx, size_t ncols,
                           Allocator *alloc)
{
    MType t = type();
    if (t == MType::STRUCT || t == MType::FUNC_HANDLE || t == MType::EMPTY)
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

        if (t == MType::CELL) {
            auto &src = cellDataVec();
            auto result = MValue::cell(newR, C);
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
            auto result = MValue::matrix(newR, C, t, alloc);
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

        if (t == MType::CELL) {
            auto &src = cellDataVec();
            auto result = MValue::cell(R, newC);
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
            auto result = MValue::matrix(R, newC, t, alloc);
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

void MValue::indexDelete3D(const size_t *rowIdx, size_t nrows,
                           const size_t *colIdx, size_t ncols,
                           const size_t *pageIdx, size_t npages,
                           Allocator *alloc)
{
    MType t = type();
    if (t == MType::STRUCT || t == MType::FUNC_HANDLE || t == MType::EMPTY)
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

        if (t == MType::CELL) {
            auto result = MValue::cell3D(R, C, newP);
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
            auto result = MValue::matrix3d(R, C, newP, t, alloc);
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

        if (t == MType::CELL) {
            auto result = MValue::cell3D(newR, C, P);
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
            auto result = MValue::matrix3d(newR, C, P, t, alloc);
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

        if (t == MType::CELL) {
            auto result = MValue::cell3D(R, newC, P);
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
            auto result = MValue::matrix3d(R, newC, P, t, alloc);
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

MValue MValue::complexScalar(Complex v, Allocator *alloc)
{
    MValue m;
    auto *h = new HeapObject();
    h->type = MType::COMPLEX;
    h->dims = {1, 1};
    h->allocator = alloc;
    h->buffer = new DataBuffer(sizeof(Complex), alloc);
    *static_cast<Complex *>(h->buffer->data()) = v;
    m.heap_ = h;
    return m;
}
MValue MValue::complexScalar(double re, double im, Allocator *alloc)
{
    return complexScalar(Complex(re, im), alloc);
}
MValue MValue::complexMatrix(size_t rows, size_t cols, Allocator *alloc)
{
    return matrix(rows, cols, MType::COMPLEX, alloc);
}

MType MValue::type() const
{
    if (heap_ == nullptr)
        return MType::DOUBLE;
    if (heap_ == emptyTag())
        return MType::EMPTY;
    if (heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return MType::LOGICAL;
    return heap_->type;
}
const Dims &MValue::dims() const
{
    if (heap_ == nullptr)
        return sScalarDims;
    if (heap_ == emptyTag())
        return sEmptyDims;
    if (heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return sScalarDims;
    return heap_->dims;
}
size_t MValue::numel() const
{
    return dims().numel();
}
bool MValue::isScalar() const
{
    if (heap_ == nullptr || heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return true;
    if (heap_ == emptyTag())
        return false;
    return heap_->dims.isScalar();
}
bool MValue::isEmpty() const
{
    if (heap_ == emptyTag())
        return true;
    if (heap_ == nullptr || heap_ == logicalTrueTag() || heap_ == logicalFalseTag())
        return false;
    return heap_->type == MType::EMPTY || heap_->dims.isEmpty();
}
bool MValue::isNumeric() const
{
    MType t = type();
    return t == MType::DOUBLE || t == MType::COMPLEX || t == MType::INT8 || t == MType::INT16
           || t == MType::INT32 || t == MType::INT64 || t == MType::UINT8 || t == MType::UINT16
           || t == MType::UINT32 || t == MType::UINT64;
}
bool MValue::isComplex() const
{
    return type() == MType::COMPLEX;
}
bool MValue::isLogical() const
{
    return type() == MType::LOGICAL;
}
bool MValue::isChar() const
{
    return type() == MType::CHAR;
}
bool MValue::isCell() const
{
    return type() == MType::CELL;
}
bool MValue::isStruct() const
{
    return type() == MType::STRUCT;
}
bool MValue::isFuncHandle() const
{
    return type() == MType::FUNC_HANDLE;
}

const void *MValue::rawData() const
{
    if (heap_ == nullptr)
        return &scalar_;
    if (isTag())
        return nullptr;
    return heap_->buffer ? heap_->buffer->data() : nullptr;
}
size_t MValue::rawBytes() const
{
    if (heap_ == nullptr)
        return sizeof(double);
    if (isTag())
        return 0;
    return heap_->buffer ? heap_->buffer->bytes() : 0;
}

const double *MValue::doubleData() const
{
    if (heap_ == nullptr)
        return &scalar_;
    if (isTag())
        throw std::runtime_error("Not a double array");
    if (heap_->type != MType::DOUBLE)
        throw std::runtime_error("Not a double array");
    return heap_->buffer ? static_cast<const double *>(heap_->buffer->data()) : nullptr;
}
const uint8_t *MValue::logicalData() const
{
    if (heap_ == logicalTrueTag()) {
        static const uint8_t t = 1;
        return &t;
    }
    if (heap_ == logicalFalseTag()) {
        static const uint8_t f = 0;
        return &f;
    }
    if (!isHeap() || heap_->type != MType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    return heap_->buffer ? static_cast<const uint8_t *>(heap_->buffer->data()) : nullptr;
}
const char *MValue::charData() const
{
    if (!isHeap() || heap_->type != MType::CHAR)
        throw std::runtime_error("Not a char array");
    return heap_->buffer ? static_cast<const char *>(heap_->buffer->data()) : nullptr;
}
double MValue::toScalar() const
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
    if (h->type == MType::DOUBLE && h->dims.isScalar())
        return *static_cast<const double *>(h->buffer->data());
    if (h->type == MType::COMPLEX && h->dims.isScalar()) {
        auto c = *static_cast<const Complex *>(h->buffer->data());
        if (c.imag() != 0.0)
            throw std::runtime_error(
                "Cannot convert complex with nonzero imaginary part to double scalar");
        return c.real();
    }
    if (h->type == MType::LOGICAL && h->dims.isScalar())
        return (double) *static_cast<const uint8_t *>(h->buffer->data());
    if (h->type == MType::CHAR && h->dims.isScalar())
        return (double) (unsigned char) *static_cast<const char *>(h->buffer->data());
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type())) + " to scalar");
}
bool MValue::toBool() const
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
    if (h->type == MType::LOGICAL && h->dims.isScalar() && h->buffer)
        return *static_cast<const uint8_t *>(h->buffer->data()) != 0;
    if (h->type == MType::DOUBLE && h->dims.isScalar() && h->buffer)
        return *static_cast<const double *>(h->buffer->data()) != 0.0;
    if (h->type == MType::COMPLEX && h->dims.isScalar() && h->buffer) {
        auto c = *static_cast<const Complex *>(h->buffer->data());
        return c.real() != 0.0 || c.imag() != 0.0;
    }
    if (h->type == MType::DOUBLE && h->buffer) {
        const double *dd = static_cast<const double *>(h->buffer->data());
        size_t n = h->dims.numel();
        for (size_t i = 0; i < n; ++i)
            if (dd[i] == 0.0)
                return false;
        return n > 0;
    }
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type())) + " to bool");
}
std::string MValue::toString() const
{
    if (isHeap() && heap_->type == MType::CHAR) {
        if (heap_->buffer)
            return std::string(static_cast<const char *>(heap_->buffer->data()),
                               heap_->dims.numel());
        return std::string(); // empty string (1x0 char)
    }
    if (isHeap() && heap_->type == MType::FUNC_HANDLE && heap_->funcName)
        return *heap_->funcName;
    throw std::runtime_error("Not a char array");
}
std::string MValue::funcHandleName() const
{
    if (isHeap() && heap_->funcName)
        return *heap_->funcName;
    return "";
}

const Complex *MValue::complexData() const
{
    if (!isHeap() || heap_->type != MType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    return heap_->buffer ? static_cast<const Complex *>(heap_->buffer->data()) : nullptr;
}
Complex MValue::toComplex() const
{
    if (heap_ == nullptr)
        return Complex(scalar_, 0.0);
    if (heap_ == logicalTrueTag())
        return Complex(1.0, 0.0);
    if (heap_ == logicalFalseTag())
        return Complex(0.0, 0.0);
    if (!isHeap() || !heap_->buffer)
        throw std::runtime_error("Cannot convert to complex");
    if (heap_->type == MType::COMPLEX && heap_->dims.isScalar())
        return *static_cast<const Complex *>(heap_->buffer->data());
    if (heap_->type == MType::DOUBLE && heap_->dims.isScalar())
        return Complex(*static_cast<const double *>(heap_->buffer->data()), 0.0);
    throw std::runtime_error("Cannot convert to complex");
}
Complex MValue::complexElem(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Complex index out of bounds");
    return complexData()[i];
}
Complex MValue::complexElem(size_t r, size_t c) const
{
    return complexData()[dims().sub2indChecked(r, c)];
}

double *MValue::doubleDataMut()
{
    if (heap_ == nullptr)
        return &scalar_;
    if (!isHeap() || heap_->type != MType::DOUBLE)
        throw std::runtime_error("Not a double array");
    detach();
    return heap_->buffer ? static_cast<double *>(heap_->buffer->data()) : nullptr;
}
uint8_t *MValue::logicalDataMut()
{
    if (!isHeap() || heap_->type != MType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    detach();
    return heap_->buffer ? static_cast<uint8_t *>(heap_->buffer->data()) : nullptr;
}
char *MValue::charDataMut()
{
    if (!isHeap() || heap_->type != MType::CHAR)
        throw std::runtime_error("Not a char array");
    detach();
    return heap_->buffer ? static_cast<char *>(heap_->buffer->data()) : nullptr;
}
void *MValue::rawDataMut()
{
    if (heap_ == nullptr)
        return &scalar_;
    if (!isHeap())
        return nullptr;
    detach();
    return heap_->buffer ? heap_->buffer->data() : nullptr;
}
Complex *MValue::complexDataMut()
{
    if (!isHeap() || heap_->type != MType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    detach();
    return heap_->buffer ? static_cast<Complex *>(heap_->buffer->data()) : nullptr;
}

void MValue::promoteToComplex(Allocator *alloc)
{
    MType t = type();
    if (t == MType::COMPLEX)
        return;
    if (t != MType::DOUBLE)
        throw std::runtime_error("Can only promote double to complex");
    size_t n = numel();
    if (heap_ == nullptr) {
        double v = scalar_;
        *this = complexScalar(Complex(v, 0.0), alloc);
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot promote to complex");
    if (!alloc)
        alloc = heap_->allocator;
    auto *newBuf = new DataBuffer(n * sizeof(Complex), alloc);
    Complex *dst = static_cast<Complex *>(newBuf->data());
    const double *src = static_cast<const double *>(heap_->buffer->data());
    for (size_t i = 0; i < n; ++i)
        dst[i] = Complex(src[i], 0.0);
    if (heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = newBuf;
    heap_->type = MType::COMPLEX;
}

double MValue::operator()(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Index out of bounds");
    return doubleData()[i];
}
double MValue::operator()(size_t r, size_t c) const
{
    return doubleData()[dims().sub2indChecked(r, c)];
}
double MValue::operator()(size_t r, size_t c, size_t p) const
{
    return doubleData()[dims().sub2indChecked(r, c, p)];
}
double &MValue::elem(size_t i)
{
    if (i >= numel())
        throw std::runtime_error("Index out of bounds");
    return doubleDataMut()[i];
}
double &MValue::elem(size_t r, size_t c)
{
    return doubleDataMut()[dims().sub2indChecked(r, c)];
}
double &MValue::elem(size_t r, size_t c, size_t p)
{
    return doubleDataMut()[dims().sub2indChecked(r, c, p)];
}

char MValue::charElem(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Char index out of bounds");
    return charData()[i];
}
char &MValue::charElemMut(size_t i)
{
    if (i >= numel())
        throw std::runtime_error("Char index out of bounds");
    return charDataMut()[i];
}

void MValue::resize(size_t newRows, size_t newCols, Allocator *alloc)
{
    if (heap_ == nullptr) {
        double v = scalar_;
        auto *h = new HeapObject();
        h->type = MType::DOUBLE;
        h->dims = {1, 1};
        h->allocator = alloc;
        h->buffer = new DataBuffer(sizeof(double), alloc);
        *static_cast<double *>(h->buffer->data()) = v;
        heap_ = h;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot resize");
    if (heap_->dims.is3D()) {
        resize3d(newRows, newCols, heap_->dims.pages(), alloc);
        return;
    }
    detach();
    if (!alloc)
        alloc = heap_->allocator;
    size_t oldR = heap_->dims.rows(), oldC = heap_->dims.cols(), es = elementSize(heap_->type),
           nb = newRows * newCols * es;
    auto *nb2 = new DataBuffer(nb, alloc);
    if (nb > 0)
        std::memset(nb2->data(), 0, nb);
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
    heap_->allocator = alloc;
    heap_->dims = {newRows, newCols};
    heap_->appendCapacity = 0;
}

void MValue::resize3d(size_t nr, size_t nc, size_t np, Allocator *alloc)
{
    if (np <= 1) {
        resize(nr, nc, alloc);
        return;
    }
    if (!isHeap())
        throw std::runtime_error("Cannot resize");
    detach();
    if (!alloc)
        alloc = heap_->allocator;
    size_t oR = heap_->dims.rows(), oC = heap_->dims.cols(), oP = heap_->dims.pages(),
           es = elementSize(heap_->type);
    size_t nb = nr * nc * np * es;
    auto *nb2 = new DataBuffer(nb, alloc);
    if (nb > 0)
        std::memset(nb2->data(), 0, nb);
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
    heap_->allocator = alloc;
    heap_->dims = {nr, nc, np};
    heap_->appendCapacity = 0;
}

void MValue::ensureSize(size_t idx, Allocator *alloc)
{
    if (heap_ == emptyTag() || (heap_ == nullptr && idx > 0)) {
        double old = (heap_ == nullptr) ? scalar_ : 0.0;
        *this = matrix(1, idx + 1, MType::DOUBLE, alloc);
        if (old != 0.0)
            static_cast<double *>(heap_->buffer->data())[0] = old;
        return;
    }
    size_t need = idx + 1;
    if (need > numel()) {
        if (dims().isVector() || dims().rows() <= 1)
            resize(1, need, alloc);
        else
            resize(dims().rows(), (need + dims().rows() - 1) / dims().rows(), alloc);
    }
}

void MValue::appendScalar(double v, Allocator *alloc)
{
    size_t oldN = numel(), newN = oldN + 1;
    if (heap_ == nullptr) {
        double old = scalar_;
        size_t cap = std::max(size_t(8), newN * 2);
        auto *h = new HeapObject();
        h->type = MType::DOUBLE;
        h->dims = {1, newN};
        h->allocator = alloc;
        h->buffer = new DataBuffer(cap * sizeof(double), alloc);
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
    if (!alloc)
        alloc = heap_->allocator;
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
    auto *nb = new DataBuffer(nc * sizeof(double), alloc);
    double *d = static_cast<double *>(nb->data());
    std::memset(d, 0, nc * sizeof(double));
    if (oldN > 0 && heap_->buffer)
        std::memcpy(d, heap_->buffer->data(), oldN * sizeof(double));
    d[oldN] = v;
    if (heap_->buffer && heap_->buffer->release())
        delete heap_->buffer;
    heap_->buffer = nb;
    heap_->allocator = alloc;
    heap_->dims = {1, newN};
    heap_->appendCapacity = nc;
}

MValue &MValue::cellAt(size_t i)
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    if (i >= heap_->cellData->size())
        throw std::runtime_error("Cell index out of bounds");
    return (*heap_->cellData)[i];
}
const MValue &MValue::cellAt(size_t i) const
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    if (i >= heap_->cellData->size())
        throw std::runtime_error("Cell index out of bounds");
    return (*heap_->cellData)[i];
}
std::vector<MValue> &MValue::cellDataVec()
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    return *heap_->cellData;
}
const std::vector<MValue> &MValue::cellDataVec() const
{
    if (!isHeap() || !heap_->cellData)
        throw std::runtime_error("Not a cell");
    return *heap_->cellData;
}

MValue &MValue::field(const std::string &n)
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    return (*heap_->structData)[n];
}
const MValue &MValue::field(const std::string &n) const
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    auto it = heap_->structData->find(n);
    if (it == heap_->structData->end())
        throw std::runtime_error("Field not found: " + n);
    return it->second;
}
bool MValue::hasField(const std::string &n) const
{
    return isHeap() && heap_->structData && heap_->structData->count(n) > 0;
}
std::map<std::string, MValue> &MValue::structFields()
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    return *heap_->structData;
}
const std::map<std::string, MValue> &MValue::structFields() const
{
    if (!isHeap() || !heap_->structData)
        throw std::runtime_error("Not a struct");
    return *heap_->structData;
}

std::string MValue::debugString() const
{
    std::ostringstream os;
    MType t = type();
    os << mtypeName(t) << " [" << dims().rows() << "x" << dims().cols();
    if (dims().is3D())
        os << "x" << dims().pages();
    os << "]";
    if (t == MType::DOUBLE && numel() <= 20 && numel() > 0) {
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
    if (t == MType::COMPLEX && numel() <= 20 && numel() > 0 && isHeap() && heap_->buffer) {
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
    if (t == MType::LOGICAL && numel() > 0) {
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
    if (t == MType::CHAR && isHeap() && heap_->buffer)
        os << " = '" << toString() << "'";
    if (t == MType::FUNC_HANDLE)
        os << " = @" << funcHandleName();
    if (t == MType::CELL && isHeap() && heap_->cellData) {
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

} // namespace mlab