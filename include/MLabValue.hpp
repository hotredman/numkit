#pragma once

#include "MLabAllocator.hpp"
#include "MLabHeapObject.hpp"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace mlab {

using Complex = std::complex<double>;

// ============================================================
// MValue — 16-byte tagged pointer value
//
// Layout:
//   double scalar_    (8 bytes) — inline scalar value
//   HeapObject *heap_ (8 bytes) — tag / heap pointer
//
// Encoding:
//   heap_ == nullptr          → double scalar (value in scalar_)
//   heap_ == kEmptyTag        → empty
//   heap_ == kLogicalTrueTag  → logical scalar true
//   heap_ == kLogicalFalseTag → logical scalar false
//   otherwise                 → heap-allocated object
// ============================================================
class MValue
{
public:
    MValue();
    ~MValue();

    MValue(const MValue &other);
    MValue &operator=(const MValue &other);
    MValue(MValue &&other) noexcept;
    MValue &operator=(MValue &&other) noexcept;

    void swap(MValue &other) noexcept;

    // ── Factories — real ─────────────────────────────────────
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
    static MValue cell3D(size_t rows, size_t cols, size_t pages);
    static MValue structure();
    static MValue funcHandle(const std::string &name, Allocator *alloc = nullptr);
    static MValue empty();
    static MValue deleted();

    // ── Factories — compound operations ────────────────────────
    // Colon range: start:stop (step=1) or start:step:stop
    static MValue colonRange(double start, double stop, Allocator *alloc = nullptr);
    static MValue colonRange(double start, double step, double stop, Allocator *alloc = nullptr);

    // Concatenation: [a, b, c] and [a; b; c]
    static MValue horzcat(const MValue *elems, size_t count, Allocator *alloc = nullptr);
    static MValue vertcat(const MValue *elems, size_t count, Allocator *alloc = nullptr);

    // ── Type-preserving indexing ─────────────────────────────
    // All methods preserve the element type (DOUBLE, COMPLEX, LOGICAL, CHAR).
    MValue elemAt(size_t linearIdx, Allocator *alloc = nullptr) const;
    MValue indexGet(const size_t *indices, size_t count, Allocator *alloc = nullptr) const;
    MValue indexGet2D(const size_t *rowIdx, size_t nrows,
                      const size_t *colIdx, size_t ncols, Allocator *alloc = nullptr) const;
    MValue indexGet3D(const size_t *rowIdx, size_t nrows,
                      const size_t *colIdx, size_t ncols,
                      const size_t *pageIdx, size_t npages, Allocator *alloc = nullptr) const;
    MValue logicalIndex(const uint8_t *mask, size_t maskLen, Allocator *alloc = nullptr) const;

    // ── Index resolution ────────────────────────────────────
    // Convert an index MValue (scalar, vector, logical mask, colon ':')
    // into a vector of 0-based indices.
    // resolveIndices: bounds-checked (for GET)
    // resolveIndicesUnchecked: no bounds check, colon requires dimSize (for SET/auto-expand)
    static std::vector<size_t> resolveIndices(const MValue &idx, size_t dimSize);
    static std::vector<size_t> resolveIndicesUnchecked(const MValue &idx);

    // ── Type-preserving indexed assignment ──────────────────
    // All methods dispatch on the array's element type.
    // val must be a scalar (broadcast) or have matching numel.
    void elemSet(size_t linearIdx, const MValue &val);
    void indexSet(const size_t *indices, size_t count, const MValue &val);
    void indexSet2D(const size_t *rowIdx, size_t nrows,
                    const size_t *colIdx, size_t ncols,
                    const MValue &val);
    void indexSet3D(const size_t *rowIdx, size_t nrows,
                    const size_t *colIdx, size_t ncols,
                    const size_t *pageIdx, size_t npages,
                    const MValue &val);

    // ── Type-preserving deletion (v(idx) = []) ─────────────
    void indexDelete(const size_t *indices, size_t count, Allocator *alloc = nullptr);
    void indexDelete2D(const size_t *rowIdx, size_t nrows,
                       const size_t *colIdx, size_t ncols,
                       Allocator *alloc = nullptr);
    void indexDelete3D(const size_t *rowIdx, size_t nrows,
                       const size_t *colIdx, size_t ncols,
                       const size_t *pageIdx, size_t npages,
                       Allocator *alloc = nullptr);

    // ── Factories — complex ──────────────────────────────────
    static MValue complexScalar(Complex v, Allocator *alloc = nullptr);
    static MValue complexScalar(double re, double im, Allocator *alloc = nullptr);
    static MValue complexMatrix(size_t rows, size_t cols, Allocator *alloc = nullptr);

    // ── Factories — string (MATLAB "..." double-quoted) ─────
    static MValue stringScalar(const std::string &s, Allocator *alloc = nullptr);
    static MValue stringArray(size_t rows, size_t cols);

    // ���─ String accessors ────────────────────────────────────
    const std::string &stringElem(size_t i) const;
    void stringElemSet(size_t i, const std::string &s);

    // ── Type queries ─────────────────────────────────────────
    MType type() const;
    const Dims &dims() const;
    size_t numel() const;
    bool isScalar() const;
    bool isEmpty() const;
    bool isNumeric() const;

    // True only for default-constructed MValue (no value assigned).
    // Unlike isEmpty(), returns false for empty matrices (A=[]) and empty strings ('').
    bool isUnset() const { return heap_ == emptyTag(); }
    // True only for variables explicitly removed via 'clear'.
    bool isDeleted() const { return heap_ == deletedTag(); }
    bool isComplex() const;
    bool isLogical() const;
    bool isChar() const;
    bool isCell() const;
    bool isStruct() const;
    bool isFuncHandle() const;
    bool isString() const;

    // ── Const raw access ─────────────────────────────────────
    const void *rawData() const;
    size_t rawBytes() const;

    // ── Const typed access — double ──────────────────────────
    const double *doubleData() const;
    const uint8_t *logicalData() const;
    const char *charData() const;
    double toScalar() const;
    bool toBool() const;
    std::string toString() const;

    // ── Const typed access — single ────────────────────────────
    const float *singleData() const;

    // ── Const typed access — integer types ───────────────────
    const int8_t  *int8Data()  const;
    const int16_t *int16Data() const;
    const int32_t *int32Data() const;
    const int64_t *int64Data() const;
    const uint16_t *uint16Data() const;
    const uint32_t *uint32Data() const;
    const uint64_t *uint64Data() const;
    // uint8 data shares accessor with logicalData() since both are uint8_t

    // ── Const typed access — complex ─────────────────────────
    const Complex *complexData() const;
    Complex toComplex() const;
    Complex complexElem(size_t i) const;
    Complex complexElem(size_t r, size_t c) const;

    // ── Mutable typed access (calls detach for COW) ──────────
    double *doubleDataMut();
    float *singleDataMut();
    uint8_t *logicalDataMut();
    char *charDataMut();
    void *rawDataMut();
    Complex *complexDataMut();
    int8_t  *int8DataMut();
    int16_t *int16DataMut();
    int32_t *int32DataMut();
    int64_t *int64DataMut();
    uint16_t *uint16DataMut();
    uint32_t *uint32DataMut();
    uint64_t *uint64DataMut();

    // ── Const indexing (column-major) ────────────────────────
    double operator()(size_t i) const;
    double operator()(size_t r, size_t c) const;
    double operator()(size_t r, size_t c, size_t p) const;

    // ── Mutable indexing (calls detach) ──────────────────────
    double &elem(size_t i);
    double &elem(size_t r, size_t c);
    double &elem(size_t r, size_t c, size_t p);

    // ── Char element access ──────────────────────────────────
    char charElem(size_t i) const;
    char &charElemMut(size_t i);

    // ── Resize ───────────────────────────────────────────────
    void resize(size_t newRows, size_t newCols, Allocator *alloc = nullptr);
    void resize3d(size_t newRows, size_t newCols, size_t newPages, Allocator *alloc = nullptr);
    void ensureSize(size_t linearIdx, Allocator *alloc = nullptr);
    void appendScalar(double v, Allocator *alloc = nullptr);

    // ── Promote double → complex ─────────────────────────────
    void promoteToComplex(Allocator *alloc = nullptr);

    // ── Cell ─────────────────────────────────────────────────
    MValue &cellAt(size_t i);
    const MValue &cellAt(size_t i) const;
    std::vector<MValue> &cellDataVec();
    const std::vector<MValue> &cellDataVec() const;

    // ── Struct ───────────────────────────────────────────────
    MValue &field(const std::string &name);
    const MValue &field(const std::string &name) const;
    bool hasField(const std::string &name) const;
    std::map<std::string, MValue> &structFields();
    const std::map<std::string, MValue> &structFields() const;

    // ── Func handle ──────────────────────────────────────────
    std::string funcHandleName() const;

    // ── Debug ────────────────────────────────────────────────
    std::string debugString() const;

    // MATLAB-style display string: "name =\n    value\n\n"
    std::string formatDisplay(const std::string &name) const;

    // ── Fast scalar access for VM ────────────────────────────
    // Caller must ensure isDoubleScalar() is true.
    bool isDoubleScalar() const { return heap_ == nullptr; }
    double scalarVal() const { return scalar_; }
    void setScalarVal(double v)
    {
        releaseHeap();
        scalar_ = v;
        heap_ = nullptr;
    }
    void setScalarFast(double v)
    {
        scalar_ = v;
        heap_ = nullptr;
    } // caller guarantees no heap to release

    // Fast logical set for VM comparison fast-path — tag-based, zero allocation
    void setLogicalFast(bool v)
    {
        scalar_ = 0.0;
        heap_ = v ? logicalTrueTag() : logicalFalseTag();
    } // caller guarantees no heap to release

    // Fast check: is this a logical scalar (tag-based)?
    bool isLogicalScalar() const { return heap_ == logicalTrueTag() || heap_ == logicalFalseTag(); }

    // Fast scalar value for both double and logical scalars
    // Caller must ensure isDoubleScalar() || isLogicalScalar()
    double fastScalarVal() const
    {
        if (heap_ == nullptr)
            return scalar_;
        // logical tag
        return heap_ == logicalTrueTag() ? 1.0 : 0.0;
    }

    // ── Ultra-fast VM hot-path accessors ─────────────────────
    // Caller must ensure isHeap() && type == DOUBLE.
    // Bypasses all safety checks and detach() for sole-owner arrays.
    bool isHeapDouble() const { return heap_ != nullptr && !isTag() && heap_->type == MType::DOUBLE; }

    const Dims &heapDims() const { return heap_->dims; }

    // Get mutable data pointer — skips detach when refcount == 1 (sole owner).
    // Caller must guarantee this is a heap DOUBLE array.
    double *doubleDataMutFast()
    {
        if (heap_->refCount.load(std::memory_order_relaxed) == 1)
            return static_cast<double *>(heap_->buffer->data());
        detach();
        return static_cast<double *>(heap_->buffer->data());
    }

    const double *doubleDataFast() const
    {
        return static_cast<const double *>(heap_->buffer->data());
    }

private:
    // ── 16-byte layout ───────────────────────────────────────
    double scalar_ = 0.0;
    HeapObject *heap_;

    // ── Sentinel tags ────────────────────────────────────────
    static HeapObject sEmptyTag;
    static HeapObject sLogicalTrue;
    static HeapObject sLogicalFalse;
    static HeapObject sDeletedTag;

    // ── Tag constants ────────────────────────────────────────
    static HeapObject *emptyTag() { return &sEmptyTag; }
    static HeapObject *logicalTrueTag() { return &sLogicalTrue; }
    static HeapObject *logicalFalseTag() { return &sLogicalFalse; }
    static HeapObject *deletedTag() { return &sDeletedTag; }

    // ── Internal helpers ─────────────────────────────────────
    bool isTag() const
    {
        return heap_ == emptyTag() || heap_ == logicalTrueTag()
            || heap_ == logicalFalseTag() || heap_ == deletedTag();
    }
    bool isHeap() const { return heap_ != nullptr && !isTag(); }

    void releaseHeap();
    void detach();
    HeapObject *mutableHeap();

    // Static dims for scalar returns
    static const Dims sScalarDims;
    static const Dims sEmptyDims;
};

} // namespace mlab