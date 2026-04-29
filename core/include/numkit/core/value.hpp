#pragma once

#include <numkit/core/heap_object.hpp>

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory_resource>
#include <string>
#include <vector>

namespace numkit {

using Complex = std::complex<double>;

// ============================================================
// Value — 16-byte tagged pointer value
//
// Layout:
//   double scalar_    (8 bytes) — inline scalar value
//   HeapObject *heap_ (8 bytes) — tag / heap pointer
//
// Encoding:
//   heap_ == nullptr           → inline double scalar (value in scalar_)
//   heap_ == emptyTag()        → empty matrix / uninitialised slot
//   heap_ == logicalTrueTag()  → logical scalar true
//   heap_ == logicalFalseTag() → logical scalar false
//   heap_ == deletedTag()      → tombstone for indexed-delete
//   otherwise                  → heap-allocated object
// ============================================================
class Value
{
public:
    Value();
    ~Value();

    Value(const Value &other);
    Value &operator=(const Value &other);
    Value(Value &&other) noexcept;
    Value &operator=(Value &&other) noexcept;

    void swap(Value &other) noexcept;

    // ── Factories — real ─────────────────────────────────────
    static Value scalar(double v, std::pmr::memory_resource *mr = nullptr);
    static Value logicalScalar(bool v, std::pmr::memory_resource *mr = nullptr);
    static Value matrix(size_t rows,
                         size_t cols,
                         ValueType t = ValueType::DOUBLE,
                         std::pmr::memory_resource *mr = nullptr);
    static Value matrix3d(size_t rows,
                           size_t cols,
                           size_t pages,
                           ValueType t = ValueType::DOUBLE,
                           std::pmr::memory_resource *mr = nullptr);
    // ND factory. dims[0..nd) is the shape, column-major. For nd <= 3 the
    // result is observably equivalent to matrix() / matrix3d(); for nd > 3
    // the heap object's Dims uses SBO storage (inline up to 4D, heap for
    // 5D+). Allocates dims[0]*dims[1]*...*dims[nd-1] * elementSize(t)
    // bytes and zero-fills.
    static Value matrixND(const size_t *dims,
                           int nd,
                           ValueType t = ValueType::DOUBLE,
                           std::pmr::memory_resource *mr = nullptr);
    static Value fromString(const std::string &s, std::pmr::memory_resource *mr = nullptr);
    static Value cell(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);
    static Value cell3D(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr = nullptr);
    // ND CELL constructor — picks 2D / 3D / true-ND backing as needed.
    static Value cellND(const size_t *dims, int nd, std::pmr::memory_resource *mr = nullptr);
    static Value structure(std::pmr::memory_resource *mr = nullptr);
    static Value funcHandle(const std::string &name, std::pmr::memory_resource *mr = nullptr);
    static Value empty();
    static Value deleted();

    // ── Factories — compound operations ────────────────────────
    // Colon range: start:stop (step=1) or start:step:stop
    static Value colonRange(double start, double stop, std::pmr::memory_resource *mr = nullptr);
    static Value colonRange(double start, double step, double stop, std::pmr::memory_resource *mr = nullptr);
    // Number of elements in the colon range (no allocation). Used by the
    // VM's lazy `for v = a:b` loop to size the iteration without
    // materialising the row vector. Throws on infinite/zero step.
    static size_t colonCount(double start, double step, double stop);

    // Concatenation: [a, b, c] and [a; b; c]
    static Value horzcat(const Value *elems, size_t count, std::pmr::memory_resource *mr = nullptr);
    static Value vertcat(const Value *elems, size_t count, std::pmr::memory_resource *mr = nullptr);

    // ── Type-preserving indexing ─────────────────────────────
    // All methods preserve the element type (DOUBLE, COMPLEX, LOGICAL, CHAR).
    Value elemAt(size_t linearIdx, std::pmr::memory_resource *mr = nullptr) const;
    // Read element idx as double — covers DOUBLE / SINGLE / LOGICAL /
    // CHAR / COMPLEX (real part) / INT8..INT64 / UINT8..UINT64.
    double elemAsDouble(size_t idx) const;
    Value indexGet(const size_t *indices, size_t count, std::pmr::memory_resource *mr = nullptr) const;
    Value indexGet2D(const size_t *rowIdx, size_t nrows,
                      const size_t *colIdx, size_t ncols, std::pmr::memory_resource *mr = nullptr) const;
    Value indexGet3D(const size_t *rowIdx, size_t nrows,
                      const size_t *colIdx, size_t ncols,
                      const size_t *pageIdx, size_t npages, std::pmr::memory_resource *mr = nullptr) const;
    /// ND subscript read for arbitrary rank. perDimIdx[i] points at a
    /// 0-based index list of length perDimCount[i] for dim i. nd ≤ 3
    /// delegates to the 1D/2D/3D fast paths. CELL is not yet supported
    /// for nd > 3.
    Value indexGetND(const size_t *const *perDimIdx,
                      const size_t *perDimCount,
                      int nd,
                      std::pmr::memory_resource *mr = nullptr) const;
    Value logicalIndex(const uint8_t *mask, size_t maskLen, std::pmr::memory_resource *mr = nullptr) const;

    // ── Index resolution ────────────────────────────────────
    // Convert an index Value (scalar, vector, logical mask, colon ':')
    // into a vector of 0-based indices.
    // resolveIndices: bounds-checked (for GET)
    // resolveIndicesUnchecked: no bounds check, colon requires dimSize (for SET/auto-expand)
    static std::vector<size_t> resolveIndices(const Value &idx, size_t dimSize);
    static std::vector<size_t> resolveIndicesUnchecked(const Value &idx);

    // ── Type-preserving indexed assignment ──────────────────
    // All methods dispatch on the array's element type.
    // val must be a scalar (broadcast) or have matching numel.
    void elemSet(size_t linearIdx, const Value &val);
    void indexSet(const size_t *indices, size_t count, const Value &val);
    void indexSet2D(const size_t *rowIdx, size_t nrows,
                    const size_t *colIdx, size_t ncols,
                    const Value &val);
    void indexSet3D(const size_t *rowIdx, size_t nrows,
                    const size_t *colIdx, size_t ncols,
                    const size_t *pageIdx, size_t npages,
                    const Value &val);
    /// ND subscript write for arbitrary rank. nd ≤ 3 delegates. val is
    /// either a scalar (broadcast) or has numel == prod(perDimCount).
    /// No auto-expand for nd > 3 — out-of-range indices throw.
    void indexSetND(const size_t *const *perDimIdx,
                    const size_t *perDimCount,
                    int nd,
                    const Value &val);

    // ── Type-preserving deletion (v(idx) = []) ─────────────
    void indexDelete(const size_t *indices, size_t count, std::pmr::memory_resource *mr = nullptr);
    void indexDelete2D(const size_t *rowIdx, size_t nrows,
                       const size_t *colIdx, size_t ncols,
                       std::pmr::memory_resource *mr = nullptr);
    void indexDelete3D(const size_t *rowIdx, size_t nrows,
                       const size_t *colIdx, size_t ncols,
                       const size_t *pageIdx, size_t npages,
                       std::pmr::memory_resource *mr = nullptr);
    // ND delete: A(i_1, ..., i_n) = []. Exactly one axis must be a
    // strict subset; all others must be the full range. Result has
    // that axis shrunk by the count of deleted indices.
    void indexDeleteND(const size_t *const *perDimIdx,
                       const size_t *perDimCount,
                       int nd,
                       std::pmr::memory_resource *mr = nullptr);

    // ── Factories — complex ──────────────────────────────────
    static Value complexScalar(Complex v, std::pmr::memory_resource *mr = nullptr);
    static Value complexScalar(double re, double im, std::pmr::memory_resource *mr = nullptr);
    static Value complexMatrix(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);

    // ── Factories — string (MATLAB "..." double-quoted) ─────
    static Value stringScalar(const std::string &s, std::pmr::memory_resource *mr = nullptr);
    static Value stringArray(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);
    static Value stringArray3D(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr = nullptr);

    // ���─ String accessors ────────────────────────────────────
    const std::string &stringElem(size_t i) const;
    void stringElemSet(size_t i, const std::string &s);

    // ── Type queries ─────────────────────────────────────────
    ValueType type() const;
    const Dims &dims() const;
    size_t numel() const;
    bool isScalar() const;
    bool isEmpty() const;
    bool isNumeric() const;

    // True only for default-constructed Value (no value assigned).
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
    const uint8_t  *uint8Data()  const;
    const uint16_t *uint16Data() const;
    const uint32_t *uint32Data() const;
    const uint64_t *uint64Data() const;

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
    uint8_t  *uint8DataMut();
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
    // Extract one row of a char matrix as a std::string. Column-major
    // stride is applied, so `reshape('abcdef',2,3).charRow(0)` is "ace".
    std::string charRow(size_t r) const;

    // ── Resize ───────────────────────────────────────────────
    void resize(size_t newRows, size_t newCols, std::pmr::memory_resource *mr = nullptr);
    void resize3d(size_t newRows, size_t newCols, size_t newPages, std::pmr::memory_resource *mr = nullptr);
    // ND resize: re-shape to `newDims` (length `nd`), preserving the
    // intersection of old and new shapes (per-axis min). Pads with 0
    // (or ' ' for CHAR). Delegates to resize/resize3d for nd ≤ 3.
    void resizeND(const size_t *newDims, int nd, std::pmr::memory_resource *mr = nullptr);
    void ensureSize(size_t linearIdx, std::pmr::memory_resource *mr = nullptr);
    void appendScalar(double v, std::pmr::memory_resource *mr = nullptr);

    // ── Promote double → complex ─────────────────────────────
    void promoteToComplex(std::pmr::memory_resource *mr = nullptr);

    // ── Cell ─────────────────────────────────────────────────
    Value &cellAt(size_t i);
    const Value &cellAt(size_t i) const;
    std::pmr::vector<Value> &cellDataVec();
    const std::pmr::vector<Value> &cellDataVec() const;

    // ── Struct ───────────────────────────────────────────────
    Value &field(const std::string &name);
    const Value &field(const std::string &name) const;
    bool hasField(const std::string &name) const;
    std::pmr::map<std::string, Value> &structFields();
    const std::pmr::map<std::string, Value> &structFields() const;

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
    bool isHeapDouble() const { return heap_ != nullptr && !isTag() && heap_->type == ValueType::DOUBLE; }

    const Dims &heapDims() const { return heap_->dims; }

    // Refcount of the underlying heap buffer. Returns 1 for scalar /
    // empty / tag values (no shared ownership possible). Used by fast
    // paths that want to mutate in-place iff they're the sole owner.
    int heapRefCount() const
    {
        if (heap_ == nullptr || isTag())
            return 1;
        return heap_->refCount.load(std::memory_order_relaxed);
    }

    // True iff heap_ points at a real (non-tag, non-null) heap object.
    // Public-facing companion to the private isHeap() used by VM
    // output-reuse fast paths.
    bool hasHeap() const
    {
        return heap_ != nullptr && !isTag();
    }

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

    // Swap data buffers between this and `other`, keeping each Value's
    // dims/type unchanged. Used by the slice-assign fast path
    // (`z(:) = expr`) to absorb a uniquely-owned temporary's buffer
    // without an O(N) memcpy. Caller must guarantee:
    //   * both MValues are heap (hasHeap() && !isTag())
    //   * both heaps are uniquely owned (heapRefCount() == 1)
    //   * both buffers are the same byte size
    // After the swap, `other` holds this Value's prior buffer; on its
    // next overwrite (typically the temp-register reuse a few bytecode
    // ops later) that buffer is freed.
    void swapHeapBufferUnchecked(Value &other) noexcept
    {
        DataBuffer *tmp = heap_->buffer;
        heap_->buffer = other.heap_->buffer;
        other.heap_->buffer = tmp;
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

} // namespace numkit