#include "MLabValue.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace mlab {

// ============================================================
// MType helpers
// ============================================================
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
        return sizeof(double);
    case MType::COMPLEX:
        return sizeof(Complex);
    case MType::LOGICAL:
        return sizeof(uint8_t);
    case MType::CHAR:
        return sizeof(char);
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

// ============================================================
// Dims
// ============================================================
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
                                 + ") exceeds array dimensions [" + std::to_string(d[0]) + "x"
                                 + std::to_string(d[1]) + "]");
    return c * d[0] + r;
}

size_t Dims::sub2indChecked(size_t r, size_t c, size_t p) const
{
    if (r >= d[0] || c >= d[1] || p >= d[2])
        throw std::runtime_error("Index (" + std::to_string(r + 1) + "," + std::to_string(c + 1)
                                 + "," + std::to_string(p + 1) + ") exceeds array dimensions ["
                                 + std::to_string(d[0]) + "x" + std::to_string(d[1]) + "x"
                                 + std::to_string(d[2]) + "]");
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

// ============================================================
// DataBuffer
// ============================================================
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
            throw std::bad_alloc();
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

// ============================================================
// MValue — buffer helpers
// ============================================================
void MValue::releaseBuffer()
{
    if (buffer_) {
        if (buffer_->release())
            delete buffer_;
        buffer_ = nullptr;
    }
}

void MValue::detach()
{
    if (!buffer_ || buffer_->refCount() <= 1)
        return;
    DataBuffer *oldBuf = buffer_;
    auto newBuf = std::unique_ptr<DataBuffer>(new DataBuffer(oldBuf->bytes(), allocator_));
    std::memcpy(newBuf->data(), oldBuf->data(), oldBuf->bytes());
    buffer_ = newBuf.release();
    if (oldBuf->release())
        delete oldBuf;
}

// ============================================================
// Constructors / destructor / swap
// ============================================================
MValue::MValue() = default;
MValue::~MValue()
{
    releaseBuffer();
}

MValue::MValue(const MValue &other)
    : type_(other.type_)
    , dims_(other.dims_)
    , buffer_(other.buffer_)
    , allocator_(other.allocator_)
    , cellData_(other.cellData_)
    , structData_(other.structData_)
    , funcHandleName_(other.funcHandleName_)
{
    if (buffer_)
        buffer_->addRef();
}

MValue &MValue::operator=(const MValue &other)
{
    if (this == &other)
        return *this;
    MValue tmp(other);
    swap(tmp);
    return *this;
}

MValue::MValue(MValue &&other)
    : type_(other.type_)
    , dims_(other.dims_)
    , buffer_(other.buffer_)
    , allocator_(other.allocator_)
    , cellData_(std::move(other.cellData_))
    , structData_(std::move(other.structData_))
    , funcHandleName_(std::move(other.funcHandleName_))
{
    other.buffer_ = nullptr;
    other.type_ = MType::EMPTY;
    other.dims_ = Dims();
}

MValue &MValue::operator=(MValue &&other)
{
    if (this == &other)
        return *this;
    MValue tmp(std::move(other));
    swap(tmp);
    return *this;
}

void MValue::swap(MValue &other) noexcept
{
    std::swap(type_, other.type_);
    std::swap(dims_, other.dims_);
    std::swap(buffer_, other.buffer_);
    std::swap(allocator_, other.allocator_);
    cellData_.swap(other.cellData_);
    structData_.swap(other.structData_);
    funcHandleName_.swap(other.funcHandleName_);
}

// ============================================================
// Factories — real
// ============================================================
MValue MValue::scalar(double v, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::DOUBLE;
    m.dims_ = {1, 1};
    m.allocator_ = alloc;
    m.buffer_ = new DataBuffer(sizeof(double), alloc);
    *static_cast<double *>(m.buffer_->data()) = v;
    return m;
}

MValue MValue::logicalScalar(bool v, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::LOGICAL;
    m.dims_ = {1, 1};
    m.allocator_ = alloc;
    m.buffer_ = new DataBuffer(sizeof(uint8_t), alloc);
    *static_cast<uint8_t *>(m.buffer_->data()) = v ? 1 : 0;
    return m;
}

MValue MValue::matrix(size_t rows, size_t cols, MType t, Allocator *alloc)
{
    MValue m;
    m.type_ = t;
    m.dims_ = {rows, cols};
    m.allocator_ = alloc;
    size_t bytes = rows * cols * elementSize(t);
    if (bytes > 0) {
        m.buffer_ = new DataBuffer(bytes, alloc);
        std::memset(m.buffer_->data(), 0, bytes);
    }
    return m;
}

MValue MValue::matrix3d(size_t rows, size_t cols, size_t pages, MType t, Allocator *alloc)
{
    MValue m;
    m.type_ = t;
    m.dims_ = {rows, cols, pages};
    m.allocator_ = alloc;
    size_t bytes = rows * cols * pages * elementSize(t);
    if (bytes > 0) {
        m.buffer_ = new DataBuffer(bytes, alloc);
        std::memset(m.buffer_->data(), 0, bytes);
    }
    return m;
}

MValue MValue::fromString(const std::string &s, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::CHAR;
    m.dims_ = {1, s.size()};
    m.allocator_ = alloc;
    if (!s.empty()) {
        m.buffer_ = new DataBuffer(s.size(), alloc);
        std::memcpy(m.buffer_->data(), s.data(), s.size());
    }
    return m;
}

MValue MValue::cell(size_t rows, size_t cols)
{
    MValue m;
    m.type_ = MType::CELL;
    m.dims_ = {rows, cols};
    m.cellData_.resize(rows * cols);
    return m;
}

MValue MValue::structure()
{
    MValue m;
    m.type_ = MType::STRUCT;
    m.dims_ = {1, 1};
    return m;
}

MValue MValue::funcHandle(const std::string &name, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::FUNC_HANDLE;
    m.dims_ = {1, 1};
    m.allocator_ = alloc;
    m.funcHandleName_ = name;
    return m;
}

MValue MValue::empty()
{
    return MValue();
}

// ============================================================
// Factories — complex
// ============================================================
MValue MValue::complexScalar(Complex v, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::COMPLEX;
    m.dims_ = {1, 1};
    m.allocator_ = alloc;
    m.buffer_ = new DataBuffer(sizeof(Complex), alloc);
    *static_cast<Complex *>(m.buffer_->data()) = v;
    return m;
}

MValue MValue::complexScalar(double re, double im, Allocator *alloc)
{
    return complexScalar(Complex(re, im), alloc);
}

MValue MValue::complexMatrix(size_t rows, size_t cols, Allocator *alloc)
{
    MValue m;
    m.type_ = MType::COMPLEX;
    m.dims_ = {rows, cols};
    m.allocator_ = alloc;
    size_t bytes = rows * cols * sizeof(Complex);
    if (bytes > 0) {
        m.buffer_ = new DataBuffer(bytes, alloc);
        std::memset(m.buffer_->data(), 0, bytes);
    }
    return m;
}

// ============================================================
// Type queries
// ============================================================
MType MValue::type() const
{
    return type_;
}
const Dims &MValue::dims() const
{
    return dims_;
}
size_t MValue::numel() const
{
    return dims_.numel();
}
bool MValue::isScalar() const
{
    return dims_.isScalar();
}
bool MValue::isEmpty() const
{
    return type_ == MType::EMPTY || dims_.isEmpty();
}
bool MValue::isNumeric() const
{
    return type_ == MType::DOUBLE || type_ == MType::COMPLEX || type_ == MType::INT8
           || type_ == MType::INT16 || type_ == MType::INT32 || type_ == MType::INT64
           || type_ == MType::UINT8 || type_ == MType::UINT16 || type_ == MType::UINT32
           || type_ == MType::UINT64;
}
bool MValue::isComplex() const
{
    return type_ == MType::COMPLEX;
}
bool MValue::isLogical() const
{
    return type_ == MType::LOGICAL;
}
bool MValue::isChar() const
{
    return type_ == MType::CHAR;
}
bool MValue::isCell() const
{
    return type_ == MType::CELL;
}
bool MValue::isStruct() const
{
    return type_ == MType::STRUCT;
}
bool MValue::isFuncHandle() const
{
    return type_ == MType::FUNC_HANDLE;
}

// ============================================================
// Const data access — raw
// ============================================================
const void *MValue::rawData() const
{
    return buffer_ ? buffer_->data() : nullptr;
}
size_t MValue::rawBytes() const
{
    return buffer_ ? buffer_->bytes() : 0;
}

// ============================================================
// Const data access — double
// ============================================================
const double *MValue::doubleData() const
{
    if (type_ != MType::DOUBLE)
        throw std::runtime_error("Not a double array");
    return buffer_ ? static_cast<const double *>(buffer_->data()) : nullptr;
}

const uint8_t *MValue::logicalData() const
{
    if (type_ != MType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    return buffer_ ? static_cast<const uint8_t *>(buffer_->data()) : nullptr;
}

const char *MValue::charData() const
{
    if (type_ != MType::CHAR)
        throw std::runtime_error("Not a char array");
    return buffer_ ? static_cast<const char *>(buffer_->data()) : nullptr;
}

double MValue::toScalar() const
{
    if (type_ == MType::DOUBLE && isScalar() && buffer_)
        return *static_cast<const double *>(buffer_->data());
    if (type_ == MType::COMPLEX && isScalar() && buffer_) {
        auto c = *static_cast<const Complex *>(buffer_->data());
        if (c.imag() != 0.0)
            throw std::runtime_error(
                "Cannot convert complex with nonzero imaginary part to double scalar");
        return c.real();
    }
    if (type_ == MType::LOGICAL && isScalar() && buffer_)
        return static_cast<double>(*static_cast<const uint8_t *>(buffer_->data()));
    if (type_ == MType::CHAR && isScalar() && buffer_)
        return static_cast<double>(
            static_cast<unsigned char>(*static_cast<const char *>(buffer_->data())));
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type_)) + " to scalar");
}

bool MValue::toBool() const
{
    if (type_ == MType::LOGICAL && isScalar() && buffer_)
        return *static_cast<const uint8_t *>(buffer_->data()) != 0;
    if (type_ == MType::DOUBLE && isScalar() && buffer_)
        return *static_cast<const double *>(buffer_->data()) != 0.0;
    if (type_ == MType::COMPLEX && isScalar() && buffer_) {
        auto c = *static_cast<const Complex *>(buffer_->data());
        return c.real() != 0.0 || c.imag() != 0.0;
    }
    if (type_ == MType::DOUBLE && buffer_) {
        const double *dd = doubleData();
        for (size_t i = 0; i < numel(); ++i)
            if (dd[i] == 0.0)
                return false;
        return numel() > 0;
    }
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type_)) + " to bool");
}

std::string MValue::toString() const
{
    if (type_ == MType::CHAR && buffer_)
        return std::string(static_cast<const char *>(buffer_->data()), dims_.numel());
    if (type_ == MType::FUNC_HANDLE)
        return funcHandleName_;
    throw std::runtime_error("Not a char array");
}

std::string MValue::funcHandleName() const
{
    return funcHandleName_;
}

// ============================================================
// Const data access — complex
// ============================================================
const Complex *MValue::complexData() const
{
    if (type_ != MType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    return buffer_ ? static_cast<const Complex *>(buffer_->data()) : nullptr;
}

Complex MValue::toComplex() const
{
    if (type_ == MType::COMPLEX && isScalar() && buffer_)
        return *static_cast<const Complex *>(buffer_->data());
    if (type_ == MType::DOUBLE && isScalar() && buffer_)
        return Complex(*static_cast<const double *>(buffer_->data()), 0.0);
    if (type_ == MType::LOGICAL && isScalar() && buffer_)
        return Complex(static_cast<double>(*static_cast<const uint8_t *>(buffer_->data())), 0.0);
    throw std::runtime_error("Cannot convert " + std::string(mtypeName(type_)) + " to complex");
}

Complex MValue::complexElem(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Complex index out of bounds");
    return complexData()[i];
}

Complex MValue::complexElem(size_t r, size_t c) const
{
    return complexData()[dims_.sub2indChecked(r, c)];
}

// ============================================================
// Mutable data access
// ============================================================
double *MValue::doubleDataMut()
{
    if (type_ != MType::DOUBLE)
        throw std::runtime_error("Not a double array");
    detach();
    return buffer_ ? static_cast<double *>(buffer_->data()) : nullptr;
}

uint8_t *MValue::logicalDataMut()
{
    if (type_ != MType::LOGICAL)
        throw std::runtime_error("Not a logical array");
    detach();
    return buffer_ ? static_cast<uint8_t *>(buffer_->data()) : nullptr;
}

char *MValue::charDataMut()
{
    if (type_ != MType::CHAR)
        throw std::runtime_error("Not a char array");
    detach();
    return buffer_ ? static_cast<char *>(buffer_->data()) : nullptr;
}

void *MValue::rawDataMut()
{
    detach();
    return buffer_ ? buffer_->data() : nullptr;
}

Complex *MValue::complexDataMut()
{
    if (type_ != MType::COMPLEX)
        throw std::runtime_error("Not a complex array");
    detach();
    return buffer_ ? static_cast<Complex *>(buffer_->data()) : nullptr;
}

// ============================================================
// Promote double → complex
// ============================================================
void MValue::promoteToComplex(Allocator *alloc)
{
    if (type_ == MType::COMPLEX)
        return;
    if (type_ != MType::DOUBLE)
        throw std::runtime_error("Can only promote double to complex");
    if (!alloc)
        alloc = allocator_;

    size_t n = numel();
    auto newBuf = std::unique_ptr<DataBuffer>(new DataBuffer(n * sizeof(Complex), alloc));
    Complex *dst = static_cast<Complex *>(newBuf->data());

    if (buffer_ && n > 0) {
        const double *src = static_cast<const double *>(buffer_->data());
        for (size_t i = 0; i < n; ++i)
            dst[i] = Complex(src[i], 0.0);
    } else {
        std::memset(dst, 0, n * sizeof(Complex));
    }

    releaseBuffer();
    buffer_ = newBuf.release();
    allocator_ = alloc;
    type_ = MType::COMPLEX;
}

// ============================================================
// Indexing — double (bounds-checked)
// ============================================================
double MValue::operator()(size_t i) const
{
    if (i >= numel())
        throw std::runtime_error("Linear index " + std::to_string(i + 1) + " exceeds array size "
                                 + std::to_string(numel()));
    return doubleData()[i];
}
double MValue::operator()(size_t r, size_t c) const
{
    return doubleData()[dims_.sub2indChecked(r, c)];
}
double MValue::operator()(size_t r, size_t c, size_t p) const
{
    return doubleData()[dims_.sub2indChecked(r, c, p)];
}

double &MValue::elem(size_t i)
{
    if (i >= numel())
        throw std::runtime_error("Linear index " + std::to_string(i + 1) + " exceeds array size "
                                 + std::to_string(numel()));
    return doubleDataMut()[i];
}
double &MValue::elem(size_t r, size_t c)
{
    return doubleDataMut()[dims_.sub2indChecked(r, c)];
}
double &MValue::elem(size_t r, size_t c, size_t p)
{
    return doubleDataMut()[dims_.sub2indChecked(r, c, p)];
}

// ============================================================
// Char element access (bounds-checked)
// ============================================================
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

// ============================================================
// Resize — 2D
// ============================================================
void MValue::resize(size_t newRows, size_t newCols, Allocator *alloc)
{
    if (dims_.is3D()) {
        resize3d(newRows, newCols, dims_.pages(), alloc);
        return;
    }

    if (!alloc)
        alloc = allocator_;
    size_t oldRows = dims_.rows(), oldCols = dims_.cols();
    size_t es = elementSize(type_);
    size_t newBytes = newRows * newCols * es;

    auto newBuf = std::unique_ptr<DataBuffer>(new DataBuffer(newBytes, alloc));
    if (newBytes > 0)
        std::memset(newBuf->data(), 0, newBytes);

    if (buffer_ && es > 0) {
        size_t copyRows = std::min(oldRows, newRows);
        size_t copyCols = std::min(oldCols, newCols);
        if (type_ == MType::DOUBLE || type_ == MType::COMPLEX) {
            const char *src = static_cast<const char *>(buffer_->data());
            char *dst = static_cast<char *>(newBuf->data());
            for (size_t c = 0; c < copyCols; ++c)
                std::memcpy(dst + c * newRows * es, src + c * oldRows * es, copyRows * es);
        } else {
            const char *src = static_cast<const char *>(buffer_->data());
            char *dst = static_cast<char *>(newBuf->data());
            for (size_t c = 0; c < copyCols; ++c)
                std::memcpy(dst + c * newRows * es, src + c * oldRows * es, copyRows * es);
        }
    }

    releaseBuffer();
    buffer_ = newBuf.release();
    allocator_ = alloc;
    dims_ = {newRows, newCols};
}

// ============================================================
// Resize — 3D
// ============================================================
void MValue::resize3d(size_t newRows, size_t newCols, size_t newPages, Allocator *alloc)
{
    if (newPages <= 1) {
        dims_.nd = 2;
        resize(newRows, newCols, alloc);
        return;
    }

    if (!alloc)
        alloc = allocator_;

    size_t oldRows = dims_.rows();
    size_t oldCols = dims_.cols();
    size_t oldPages = dims_.pages();
    size_t es = elementSize(type_);
    size_t newBytes = newRows * newCols * newPages * es;

    auto newBuf = std::unique_ptr<DataBuffer>(new DataBuffer(newBytes, alloc));
    if (newBytes > 0)
        std::memset(newBuf->data(), 0, newBytes);

    if (buffer_ && es > 0) {
        size_t copyRows = std::min(oldRows, newRows);
        size_t copyCols = std::min(oldCols, newCols);
        size_t copyPages = std::min(oldPages, newPages);

        size_t oldPageStride = oldRows * oldCols;
        size_t newPageStride = newRows * newCols;

        const char *src = static_cast<const char *>(buffer_->data());
        char *dst = static_cast<char *>(newBuf->data());
        for (size_t p = 0; p < copyPages; ++p)
            for (size_t c = 0; c < copyCols; ++c)
                std::memcpy(dst + (p * newPageStride + c * newRows) * es,
                            src + (p * oldPageStride + c * oldRows) * es,
                            copyRows * es);
    }

    releaseBuffer();
    buffer_ = newBuf.release();
    allocator_ = alloc;
    dims_ = {newRows, newCols, newPages};
}

// ============================================================
// ensureSize
// ============================================================
void MValue::ensureSize(size_t linearIdx, Allocator *alloc)
{
    if (type_ == MType::EMPTY) {
        type_ = MType::DOUBLE;
        dims_ = {1, 1};
        if (!alloc)
            alloc = allocator_;
        buffer_ = new DataBuffer(sizeof(double), alloc);
        std::memset(buffer_->data(), 0, sizeof(double));
        allocator_ = alloc;
    }
    size_t needed = linearIdx + 1;
    if (needed > numel()) {
        if (dims_.isVector() || dims_.rows() <= 1)
            resize(1, needed, alloc);
        else {
            size_t newCols = (needed + dims_.rows() - 1) / dims_.rows();
            resize(dims_.rows(), newCols, alloc);
        }
    }
}

// ============================================================
// Cell
// ============================================================
MValue &MValue::cellAt(size_t i)
{
    if (type_ != MType::CELL)
        throw std::runtime_error("Not a cell");
    if (i >= cellData_.size())
        throw std::runtime_error("Cell index out of bounds");
    return cellData_[i];
}

const MValue &MValue::cellAt(size_t i) const
{
    if (type_ != MType::CELL)
        throw std::runtime_error("Not a cell");
    if (i >= cellData_.size())
        throw std::runtime_error("Cell index out of bounds");
    return cellData_[i];
}

std::vector<MValue> &MValue::cellDataVec()
{
    return cellData_;
}
const std::vector<MValue> &MValue::cellDataVec() const
{
    return cellData_;
}

// ============================================================
// Struct
// ============================================================
MValue &MValue::field(const std::string &name)
{
    if (type_ != MType::STRUCT)
        throw std::runtime_error("Not a struct");
    return structData_[name];
}

const MValue &MValue::field(const std::string &name) const
{
    if (type_ != MType::STRUCT)
        throw std::runtime_error("Not a struct");
    auto it = structData_.find(name);
    if (it == structData_.end())
        throw std::runtime_error("Field not found: " + name);
    return it->second;
}

bool MValue::hasField(const std::string &name) const
{
    return structData_.count(name) > 0;
}
std::map<std::string, MValue> &MValue::structFields()
{
    return structData_;
}
const std::map<std::string, MValue> &MValue::structFields() const
{
    return structData_;
}

// ============================================================
// Debug
// ============================================================
std::string MValue::debugString() const
{
    std::ostringstream os;
    os << mtypeName(type_) << " [" << dims_.rows() << "x" << dims_.cols();
    if (dims_.is3D())
        os << "x" << dims_.pages();
    os << "]";
    if (type_ == MType::DOUBLE && numel() <= 20 && numel() > 0 && buffer_) {
        os << " = ";
        const double *dd = doubleData();
        if (isScalar()) {
            os << dd[0];
        } else {
            os << "[";
            for (size_t i = 0; i < numel(); ++i) {
                if (i)
                    os << ", ";
                os << dd[i];
            }
            os << "]";
        }
    }
    if (type_ == MType::COMPLEX && numel() <= 20 && numel() > 0 && buffer_) {
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
    if (type_ == MType::LOGICAL && numel() <= 20 && numel() > 0 && buffer_) {
        os << " = ";
        const uint8_t *ld = logicalData();
        if (isScalar()) {
            os << (ld[0] ? "true" : "false");
        } else {
            os << "[";
            for (size_t i = 0; i < numel(); ++i) {
                if (i)
                    os << ", ";
                os << (ld[i] ? "1" : "0");
            }
            os << "]";
        }
    }
    if (type_ == MType::CHAR && buffer_)
        os << " = '" << toString() << "'";
    if (type_ == MType::FUNC_HANDLE)
        os << " = @" << funcHandleName_;
    if (type_ == MType::CELL) {
        os << " {";
        for (size_t i = 0; i < cellData_.size() && i < 10; ++i) {
            if (i)
                os << ", ";
            os << cellData_[i].debugString();
        }
        if (cellData_.size() > 10)
            os << ", ...";
        os << "}";
    }
    return os.str();
}

} // namespace mlab
