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
// Horizontal concatenation: [a, b, c]
// ============================================================
MValue MValue::horzcat(const MValue *elems, size_t count, Allocator *alloc)
{
    // Check for string concatenation
    bool hasChar = false;
    for (size_t i = 0; i < count; ++i) {
        if (!elems[i].isEmpty() && elems[i].type() == MType::CHAR) {
            hasChar = true;
            break;
        }
    }

    if (hasChar) {
        std::string result;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].type() == MType::CHAR)
                result += elems[i].toString();
            else if (elems[i].isDoubleScalar())
                result += static_cast<char>(static_cast<int>(elems[i].toScalar()));
            else
                throw std::runtime_error("Cannot concatenate char and non-char arrays");
        }
        return MValue::fromString(result, alloc);
    }

    // Determine output dimensions
    size_t totalCols = 0;
    size_t rows = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        if (elems[i].isDoubleScalar()) {
            totalCols++;
            if (!rows)
                rows = 1;
            continue;
        }
        auto &dims = elems[i].dims();
        totalCols += dims.cols();
        if (!rows)
            rows = dims.rows();
        else if (rows != dims.rows() && dims.rows() != 1 && rows != 1)
            throw std::runtime_error(
                "Dimensions of arrays being concatenated are not consistent");
        if (dims.rows() > 1)
            rows = dims.rows();
    }
    if (!rows)
        rows = 1;

    if (rows == 1) {
        // Row vector — flat copy
        size_t total = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            total += elems[i].isDoubleScalar() ? 1 : elems[i].numel();
        }
        auto result = MValue::matrix(1, total, MType::DOUBLE, alloc);
        double *d = result.doubleDataMut();
        size_t pos = 0;
        for (size_t i = 0; i < count; ++i) {
            if (elems[i].isEmpty())
                continue;
            if (elems[i].isDoubleScalar()) {
                d[pos++] = elems[i].scalarVal();
                continue;
            }
            const double *src = elems[i].doubleData();
            size_t n = elems[i].numel();
            std::copy(src, src + n, d + pos);
            pos += n;
        }
        return result;
    }

    // 2D — column-major copy
    auto result = MValue::matrix(rows, totalCols, MType::DOUBLE, alloc);
    double *d = result.doubleDataMut();
    size_t colOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        if (elems[i].isDoubleScalar()) {
            d[colOff * rows] = elems[i].scalarVal();
            colOff++;
            continue;
        }
        auto &dims = elems[i].dims();
        const double *src = elems[i].doubleData();
        for (size_t c = 0; c < dims.cols(); ++c)
            for (size_t r = 0; r < dims.rows(); ++r)
                d[r + (colOff + c) * rows] = src[r + c * dims.rows()];
        colOff += dims.cols();
    }
    return result;
}

// ============================================================
// Vertical concatenation: [a; b; c]
// ============================================================
MValue MValue::vertcat(const MValue *elems, size_t count, Allocator *alloc)
{
    size_t totalRows = 0, cols = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        size_t eCols, eRows;
        if (elems[i].isDoubleScalar()) {
            eRows = 1;
            eCols = 1;
        } else {
            auto &dims = elems[i].dims();
            eRows = dims.rows();
            eCols = dims.cols();
        }
        totalRows += eRows;
        if (!cols)
            cols = eCols;
        else if (cols != eCols && eCols != 1 && cols != 1)
            throw std::runtime_error(
                "Dimensions of arrays being concatenated are not consistent");
        if (eCols > 1)
            cols = eCols;
    }
    if (!cols) {
        return MValue::empty();
    }
    auto result = MValue::matrix(totalRows, cols, MType::DOUBLE, alloc);
    double *d = result.doubleDataMut();
    size_t rowOff = 0;
    for (size_t i = 0; i < count; ++i) {
        if (elems[i].isEmpty())
            continue;
        if (elems[i].isDoubleScalar()) {
            d[rowOff++] = elems[i].scalarVal();
            continue;
        }
        auto &dims = elems[i].dims();
        const double *src = elems[i].doubleData();
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < dims.rows(); ++r)
                d[(rowOff + r) + c * totalRows] = src[r + c * dims.rows()];
        rowOff += dims.rows();
    }
    return result;
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