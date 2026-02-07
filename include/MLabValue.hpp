#pragma once

#include "MLabAllocator.hpp"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mlab {

using Complex = std::complex<double>;

enum class MType {
    EMPTY,
    DOUBLE,
    COMPLEX,
    LOGICAL,
    CHAR,
    CELL,
    STRUCT,
    FUNC_HANDLE,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64
};

const char *mtypeName(MType t);
size_t elementSize(MType t);

struct Dims
{
    size_t d[3] = {0, 0, 1};
    int nd = 2;

    Dims();
    Dims(size_t r, size_t c);
    Dims(size_t r, size_t c, size_t p);

    int ndims() const;
    size_t rows() const;
    size_t cols() const;
    size_t pages() const;
    size_t numel() const;
    bool isScalar() const;
    bool isEmpty() const;
    bool isVector() const;
    bool is3D() const;

    size_t dimSize(int dim) const;
    size_t sub2ind(size_t r, size_t c) const;
    size_t sub2ind(size_t r, size_t c, size_t p) const;
    size_t sub2indChecked(size_t r, size_t c) const;
    size_t sub2indChecked(size_t r, size_t c, size_t p) const;

    bool operator==(const Dims &o) const;
    bool operator!=(const Dims &o) const;
};

class DataBuffer
{
public:
    DataBuffer(size_t bytes, Allocator *alloc);
    ~DataBuffer();

    DataBuffer(const DataBuffer &) = delete;
    DataBuffer &operator=(const DataBuffer &) = delete;

    void *data() { return data_; }
    const void *data() const { return data_; }
    size_t bytes() const { return bytes_; }
    Allocator *allocator() const { return allocator_; }

    void addRef();
    bool release();
    int refCount() const;

private:
    void *data_ = nullptr;
    size_t bytes_ = 0;
    std::atomic<int> refCount_{1};
    Allocator *allocator_ = nullptr;
};

class MValue
{
public:
    MValue();
    ~MValue();

    MValue(const MValue &other);
    MValue &operator=(const MValue &other);
    MValue(MValue &&other);
    MValue &operator=(MValue &&other);

    void swap(MValue &other) noexcept;

    // Factories — real
    static MValue scalar(double v, Allocator *alloc = nullptr);
    static MValue logicalScalar(bool v, Allocator *alloc = nullptr);
    static MValue matrix(size_t rows,
                         size_t cols,
                         MType t = MType::DOUBLE,
                         Allocator *alloc = nullptr);
    static MValue matrix3d(size_t rows,
                           size_t cols,
                           size_t pages,
                           MType t = MType::DOUBLE,
                           Allocator *alloc = nullptr);
    static MValue fromString(const std::string &s, Allocator *alloc = nullptr);
    static MValue cell(size_t rows, size_t cols);
    static MValue structure();
    static MValue funcHandle(const std::string &name, Allocator *alloc = nullptr);
    static MValue empty();

    // Factories — complex
    static MValue complexScalar(Complex v, Allocator *alloc = nullptr);
    static MValue complexScalar(double re, double im, Allocator *alloc = nullptr);
    static MValue complexMatrix(size_t rows, size_t cols, Allocator *alloc = nullptr);

    // Type queries
    MType type() const;
    const Dims &dims() const;
    size_t numel() const;
    bool isScalar() const;
    bool isEmpty() const;
    bool isNumeric() const;
    bool isComplex() const;
    bool isLogical() const;
    bool isChar() const;
    bool isCell() const;
    bool isStruct() const;
    bool isFuncHandle() const;

    // Const raw access
    const void *rawData() const;
    size_t rawBytes() const;

    // Const typed access — double
    const double *doubleData() const;
    const uint8_t *logicalData() const;
    const char *charData() const;
    double toScalar() const;
    bool toBool() const;
    std::string toString() const;

    // Const typed access — complex
    const Complex *complexData() const;
    Complex toComplex() const;
    Complex complexElem(size_t i) const;
    Complex complexElem(size_t r, size_t c) const;

    // Mutable typed access (calls detach)
    double *doubleDataMut();
    uint8_t *logicalDataMut();
    char *charDataMut();
    void *rawDataMut();
    Complex *complexDataMut();

    // Const indexing (column-major)
    double operator()(size_t i) const;
    double operator()(size_t r, size_t c) const;
    double operator()(size_t r, size_t c, size_t p) const;

    // Mutable indexing (calls detach)
    double &elem(size_t i);
    double &elem(size_t r, size_t c);
    double &elem(size_t r, size_t c, size_t p);

    // Char element access
    char charElem(size_t i) const;
    char &charElemMut(size_t i);

    // Resize
    void resize(size_t newRows, size_t newCols, Allocator *alloc = nullptr);
    void resize3d(size_t newRows, size_t newCols, size_t newPages, Allocator *alloc = nullptr);
    void ensureSize(size_t linearIdx, Allocator *alloc = nullptr);

    // Promote double → complex
    void promoteToComplex(Allocator *alloc = nullptr);

    // Cell
    MValue &cellAt(size_t i);
    const MValue &cellAt(size_t i) const;
    std::vector<MValue> &cellDataVec();
    const std::vector<MValue> &cellDataVec() const;

    // Struct
    MValue &field(const std::string &name);
    const MValue &field(const std::string &name) const;
    bool hasField(const std::string &name) const;
    std::map<std::string, MValue> &structFields();
    const std::map<std::string, MValue> &structFields() const;

    // Func handle
    std::string funcHandleName() const;

    // Debug
    std::string debugString() const;

private:
    MType type_ = MType::EMPTY;
    Dims dims_;
    DataBuffer *buffer_ = nullptr;
    Allocator *allocator_ = nullptr;

    std::vector<MValue> cellData_;
    std::map<std::string, MValue> structData_;
    std::string funcHandleName_;

    void detach();
    void releaseBuffer();
};

} // namespace mlab
