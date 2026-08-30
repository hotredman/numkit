#pragma once

#include <numkit/value/heap_object.hpp>
#include <numkit/value/span.hpp>

#include <array>
#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <memory_resource>
#include <string>
#include <vector>

namespace numkit {

using Complex = std::complex<double>;

namespace detail {
/// Compile-time map from C++ type → numkit ValueType. Primary template
/// is left undefined: instantiating it with an unsupported T yields a
/// hard compile error pointing at the offending call site.
template<class T> struct value_type_for;

template<> struct value_type_for<double>        { static constexpr ValueType type = ValueType::DOUBLE;  };
template<> struct value_type_for<float>         { static constexpr ValueType type = ValueType::SINGLE;  };
template<> struct value_type_for<std::int8_t>   { static constexpr ValueType type = ValueType::INT8;    };
template<> struct value_type_for<std::int16_t>  { static constexpr ValueType type = ValueType::INT16;   };
template<> struct value_type_for<std::int32_t>  { static constexpr ValueType type = ValueType::INT32;   };
template<> struct value_type_for<std::int64_t>  { static constexpr ValueType type = ValueType::INT64;   };
template<> struct value_type_for<std::uint8_t>  { static constexpr ValueType type = ValueType::UINT8;   };
template<> struct value_type_for<std::uint16_t> { static constexpr ValueType type = ValueType::UINT16;  };
template<> struct value_type_for<std::uint32_t> { static constexpr ValueType type = ValueType::UINT32;  };
template<> struct value_type_for<std::uint64_t> { static constexpr ValueType type = ValueType::UINT64;  };
template<> struct value_type_for<std::complex<double>> { static constexpr ValueType type = ValueType::COMPLEX; };
} // namespace detail

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
/// @brief Core MATLAB-compatible multidimensional array and scalar value type.
///
/// Represents scalars, matrices, multi-dimensional arrays, strings, cell arrays, and structures
/// with copy-on-write (COW) semantics and polymorphic PMR memory resource allocation.
class Value
{
public:
    /// @brief Constructs an empty `0 x 0` double matrix (`[]`).
    Value();
    ~Value();

    Value(const Value &other);
    Value &operator=(const Value &other);
    Value(Value &&other) noexcept;
    Value &operator=(Value &&other) noexcept;

    void swap(Value &other) noexcept;

    // ── Ergonomic ctors — owning, COPY ────────────────────────
    //
    // Build a 1×N row vector from common C++ sources. All four COPY
    // the input bytes into a fresh buffer on `mr` (nullptr → process
    // default resource). Reshape afterwards if you need a matrix / ND.
    //
    // T must be one of: double, float, int8..int64, uint8..uint64,
    // std::complex<double>. Other types fail to compile via
    // detail::value_type_for.
    //
    // The initializer_list overload is non-template (always double) so
    // that `Value v = {1, 2, 3};` works without int → DOUBLE surprises
    // from template type deduction. For other types use the vector /
    // array / Span overloads.

    /// Row vector from a brace-enclosed list of doubles.
    /// @code  Value v = {1.0, 2.0, 3.0};  // 1×3 DOUBLE  @endcode
    Value(std::initializer_list<double> il,
          std::pmr::memory_resource *mr = nullptr);

    /// Row vector from a contiguous span.
    template<class T>
    Value(Span<const T> data,
          std::pmr::memory_resource *mr = nullptr);

    /// Row vector from std::vector — copies all elements.
    template<class T>
    Value(const std::vector<T> &v,
          std::pmr::memory_resource *mr = nullptr);

    /// Row vector from std::array — copies all elements.
    template<class T, std::size_t N>
    Value(const std::array<T, N> &a,
          std::pmr::memory_resource *mr = nullptr);

    /// Non-owning view over an externally-owned buffer.
    ///
    /// @warning  Caller MUST keep `data` alive for as long as the
    ///           returned Value (or any copy of it) is in use. The
    ///           Value never frees `data`. Mutating via doubleDataMut
    ///           etc. silently clones into a fresh owning buffer (COW).
    ///
    /// @code
    /// double buf[1024];
    /// Value v = Value::view(buf, ValueType::DOUBLE, Dims{1, 1024});
    /// @endcode
    static Value view(const void *data,
                      ValueType type,
                      Dims dims);

    /// MATLAB-style empty matrix sentinel — a 0×0 DOUBLE value.
    ///
    /// Use as the default for optional `const Value &` parameters in
    /// public library API. Equivalent to MATLAB `[]`. Inside the
    /// function, check via `x.isEmpty()` (or `x.numel() == 0`) which
    /// also catches truly Unset Values, giving uniform MATLAB-style
    /// "use default if empty" semantics.
    ///
    ///   isEmpty() = true
    ///   isUnset() = false       (distinct from default-constructed Value)
    ///   type()    = DOUBLE
    ///   dims()    = {0, 0}
    static const Value Empty;

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
    // CSL (comma-separated list) — a transient n-element value-list (see ValueType::CSL).
    // Storage mirrors a 1xn CELL (HeapObject::cellData); only the type tag differs.
    static Value csl(size_t n, std::pmr::memory_resource *mr = nullptr);
    static Value structure(std::pmr::memory_resource *mr = nullptr);
    // N×M struct array — every element is an independent map<string, Value>.
    // numel() == rows*cols == size of the underlying structArray vector.
    // Field set is allowed to differ across elements; MATLAB-compatible
    // builtins (struct, fieldnames, ...) maintain the uniform-field
    // invariant on creation.
    static Value structArray(size_t rows, size_t cols,
                              std::pmr::memory_resource *mr = nullptr);
    static Value funcHandle(const std::string &name, std::pmr::memory_resource *mr = nullptr);
    // ── OBJECT (class instance) — see object.hpp / OBJECT_MODEL.md ──
    // Wrap pre-built instance state as an instance of `className`.
    // `isHandle` is the class's handle flag and drives COW (clone shares
    // state for handle classes, deep-copies for value classes).
    static Value object(const std::string &className,
                        std::shared_ptr<ObjectState> state, bool isHandle,
                        std::pmr::memory_resource *mr = nullptr);
    static Value objectArray(const std::string &className, const Dims &dims,
                             std::vector<std::shared_ptr<ObjectState>> states, bool isHandle,
                             std::pmr::memory_resource *mr = nullptr);
    /// Returns a default-constructed (Unset) Value — NOT a MATLAB
    /// empty matrix despite the name. New code should use
    /// `Value::Empty` for MATLAB-style empty (0×0 DOUBLE) or
    /// `Value()` directly for the unset sentinel.
    [[deprecated("Use Value::Empty (MATLAB 0x0 DOUBLE) or Value() (unset sentinel)")]]
    static Value empty();
    static Value deleted();

    // ── Factories — compound operations ────────────────────────
    // Colon range: start:stop (step=1) or start:step:stop
    static Value colonRange(double start, double stop, std::pmr::memory_resource *mr = nullptr);
    static Value colonRange(double start, double step, double stop, std::pmr::memory_resource *mr = nullptr);
    // Typed colon range: same as above but output is class `t`
    // (single / int8..uint64 / logical). Values are computed in double
    // then cast to `t`. DOUBLE → identical to the untyped overload.
    static Value colonRangeTyped(double start, double stop, ValueType t,
                                  std::pmr::memory_resource *mr = nullptr);
    static Value colonRangeTyped(double start, double step, double stop,
                                  ValueType t, std::pmr::memory_resource *mr = nullptr);
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
    // Reshape to rows x cols (numel must match), column-major-preserving. The
    // L0 primitive behind the reshape builtin and the `x(:)` linear-index
    // column form: OBJECT -> objectReshape, CELL/STRING copy element-wise,
    // contiguous types memcpy the buffer.
    Value reshape(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr) const;
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

    // Validate a single 1-based scalar subscript (paren OR brace) and return
    // it 0-based. Throws the MATLAB-exact "Array indices must be positive
    // integers or logical values." on fractional, non-positive, NaN or Inf
    // input. Does NOT bounds-check — callers that auto-grow accept any
    // in-range positive integer. Single source of truth for scalar-subscript
    // validation across both engines.
    static size_t checkedScalarIndex(double oneBased);

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
    bool isObject() const;
    bool isCsl() const;  // comma-separated list (transient value-list); see ValueType::CSL

    // ── OBJECT accessors (see object.hpp) ────────────────────
    // Class name of an OBJECT ("" otherwise). objectIsHandle: the
    // class's reference-semantics flag.
    std::string objectClassName() const;
    bool objectIsHandle() const;
    // Number of elements in an OBJECT array (0 if not an object; 1 for a
    // scalar object). Shape lives in dims()/size() like any other array.
    size_t objectCount() const;
    // Read-only instance state of the FIRST element (null if not an
    // object / empty). For scalar objects this is the only element.
    const ObjectState *objectStateConst() const;
    // Mutable instance state for writers. Detaches (COW) first so the
    // value/handle clone rule applies, then returns the state to mutate
    // (uniquely-owned for a value class; the shared one for a handle).
    ObjectState *objectStateMut();
    // Per-element accessors for object arrays (null if i out of range).
    const ObjectState *objectStateAt(size_t i) const;
    ObjectState *objectStateMutAt(size_t i);

    // ── Builtin object-array indexing (no custom subsref/subsasgn) ──
    // Select elements `idxs` (0-based linear) into a new object array of
    // the same class. One index → a 1×1 scalar object; several → a 1×N
    // row. Applies the value/handle rule per element (value classes get
    // independent deep copies, handle classes alias), so a value-class
    // element read is safe to mutate. Throws on out-of-range index.
    Value objectSubArray(const std::vector<size_t> &idxs,
                         std::pmr::memory_resource *mr = nullptr) const;
    // Reshape an object array to `newDims` (same numel) — element order is
    // column-major-preserving, like reshape on any array. Applies the
    // per-element value/handle rule. Used by the reshape builtin.
    Value objectReshape(const Dims &newDims, std::pmr::memory_resource *mr = nullptr) const;
    // 2-D transpose of an object array (R×C → C×R). Used by the builtin
    // transpose/ctranspose for objects with no class overload.
    Value objectTranspose(std::pmr::memory_resource *mr = nullptr) const;
    // General element-rearrange primitive: build an object array of shape
    // `resultDims` by gathering source states at the column-major linear
    // indices `srcLinear` (length == resultDims.numel()), applying the
    // per-element value/handle rule. Underlies object indexing, reshape and
    // transpose; exposed for builtin rearrangers (repmat / flip / cat).
    Value objectGather(const size_t *srcLinear, const Dims &resultDims,
                       std::pmr::memory_resource *mr = nullptr) const;
    // Concatenate object `parts` (same class) by appending their states in
    // column-major order into an array of shape `outDims` (numel must equal
    // the total element count). value/handle rule per element. Used by
    // cat(dim≥3, ...) for objects; empties are skipped.
    static Value objectConcatN(const Value *parts, size_t count, const Dims &outDims,
                               std::pmr::memory_resource *mr = nullptr);
    // Assign scalar object `elem` into linear slot `idx`, growing this
    // (1-D row vector) and gap-filling new slots with independent copies
    // of `fill` (typically a default-constructed object). An empty/unset/
    // non-object receiver becomes a fresh object array of elem's class.
    // Value class → store a deep copy of elem; handle class → alias elem.
    // Detaches (COW) first. For the no-custom-subsasgn builtin path.
    // Linear slice assignment: write `rhs` into the linear positions `pos`
    // (0-based). `rhs` is either a scalar object (broadcast to every target)
    // or an object array with numel == pos.size() (placed in order). Grows
    // as a vector when a position is past the end (a proper matrix errors);
    // an empty/unset receiver becomes a fresh object array of rhs's class.
    // value class → deep copy per element, handle class → alias.
    void objectAssignLinear(const std::vector<size_t> &pos, const Value &rhs,
                            const Value &fill, std::pmr::memory_resource *mr = nullptr);
    // N-D slice assignment: `perDim[d]` is the 0-based index list for
    // dimension d; targets are the column-major cartesian product. `rhs`
    // broadcasts (scalar) or matches the target count. Grows any rank and
    // re-lays-out column-major, default-filling new slots with `fill`.
    void objectAssignND(const std::vector<std::vector<size_t>> &perDim, const Value &rhs,
                        const Value &fill, std::pmr::memory_resource *mr = nullptr);

    // ── Const raw access ─────────────────────────────────────
    const void *rawData() const;
    size_t rawBytes() const;
    // whos-correct deep size: a struct/cell reports the sum of all its
    // contents (recursively) plus its own schema (field-name bytes), instead
    // of 0. Leaf arrays return rawBytes(). Use this for display (whos /
    // workspace / struct inspector); use rawBytes() for flat-buffer ops.
    size_t deepBytes() const;

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
    // ── CSL (comma-separated list) ───────────────────────────
    // Element access + count for a transient value-list. Storage is the
    // shared cellData backing, so cellAt() also works; these read the count
    // without the isCell() type assumption and document CSL intent at call sites.
    size_t cslCount() const;
    Value &cslAt(size_t i) { return cellAt(i); }
    const Value &cslAt(size_t i) const { return cellAt(i); }
    // Grow a cell so subscript `coords` (length `nd`) is in bounds,
    // preserving existing contents and lifting the rank if needed.
    // Coerces an empty/unset receiver to a cell. Returns the column-major
    // linear index of `coords` in the (grown) cell.
    size_t growCellTo(const size_t *coords, int nd,
                      std::pmr::memory_resource *mr = nullptr);
    std::pmr::vector<Value> &cellDataVec();
    const std::pmr::vector<Value> &cellDataVec() const;

    // ── Struct ───────────────────────────────────────────────
    Value &field(const std::string &name);
    const Value &field(const std::string &name) const;
    bool hasField(const std::string &name) const;
    std::pmr::map<std::string, Value> &structFields();
    const std::pmr::map<std::string, Value> &structFields() const;

    // BUG #15 fix: insertion-order tracking for fieldnames(). Use
    // setField / setFieldAll for new field assignments so the order is
    // preserved; the raw map[] also works (alphabetical via std::map)
    // but won't update the order vector. removeField clears across
    // every struct array element and the order tracker.
    //
    // setField(linear, name, value): assign to element `linear` only.
    // setFieldAll(name, value): assign across the whole struct array.
    // Both append `name` to fieldOrder if not already present.
    void setField(size_t linearIdx, const std::string &name, const Value &v);
    void setFieldAll(const std::string &name, const Value &v);
    void removeField(const std::string &name);
    // Returns the field names in MATLAB insertion order (preferred).
    // Falls back to alphabetical (struct map iteration) if fieldOrder
    // is missing — happens for legacy/cloned structs.
    std::vector<std::string> fieldNamesInOrder() const;

    // True for struct arrays (numel > 1). isStruct() && !isStructArray()
    // means a single struct (the legacy code path).
    bool isStructArray() const;
    // Mutable / const access to the i-th element's field map. Caller must
    // ensure i < numel(). Throws when called on a single struct (use
    // structFields() there instead).
    std::pmr::map<std::string, Value> &structArrayElem(size_t i);

    // Grow a struct array so that linear index `idx` is in bounds.
    // No-op if already large enough. Coerces the receiver to a fresh
    // struct array if it's empty/unset. Row-vector / column-vector
    // shape preserved; default to row vector for empty / 1×1 starts.
    // Does not insert any field into the new slots — they're empty.
    void growStructArrayTo(size_t idx, std::pmr::memory_resource *mr = nullptr);
    // N-D analogue: grow the struct array so subscript `coords` (length
    // `nd`) is in bounds, preserving existing elements at their
    // column-major positions and lifting the rank if needed. Coerces an
    // empty/unset receiver to a struct array. Returns the column-major
    // linear index of `coords` in the (grown) array.
    size_t growStructArrayND(const size_t *coords, int nd,
                             std::pmr::memory_resource *mr = nullptr);
    const std::pmr::map<std::string, Value> &structArrayElem(size_t i) const;

    // ── Func handle ──────────────────────────────────────────
    std::string funcHandleName() const;
    /// Reconstructed source text of an anonymous-function handle (e.g.
    /// "@(x)x+1"), set at creation; empty for named handles. Used by func2str.
    std::string funcHandleSource() const;
    /// Attach the anonymous-function source text to a freshly-created handle.
    void setFuncHandleSource(const std::string &source);

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
    // Concatenate same-class object elements into a 1×N row / N×1 column
    // object array (per-element value/handle rule). Used by horzcat/vertcat.
    static Value concatObjects(const Value *elems, size_t count, bool vertical,
                               std::pmr::memory_resource *mr);
    // Coerce a non-object receiver into a fresh 0×0 object array of `proto`'s
    // class. No-op when already an object. Used by the slice-assign helpers.
    void objectEnsureArrayLike(const Value &proto, std::pmr::memory_resource *mr);

    // Static dims for scalar returns
    static const Dims sScalarDims;
    static const Dims sEmptyDims;

    // Shared body of the ergonomic ctors. Allocates a 1×count row of
    // `type` on `mr` and memcpy's `count * elemSize` bytes from `src`.
    static Value makeContiguous(const void *src, size_t count,
                                ValueType type, size_t elemSize,
                                std::pmr::memory_resource *mr);
};

// MATLAB narrows a complex result whose imaginary part is all-zero back to a real
// double — for the RESULTS of operations (arithmetic, indexing, reductions, …),
// NOT the complex() constructor or a pure structural reshape/transpose/concat. A
// NaN imaginary part keeps it complex. No-op unless COMPLEX with every imaginary
// component exactly zero. bugs/math/complex-zero-imag-narrowing.md.
Value narrowComplex(Value v, std::pmr::memory_resource *mr);

// Collapse a comma-separated list reaching a SINGLE-VALUE context: a CSL with exactly
// one element yields that element; 0 or >1 elements throw (MATLAB "not enough" / "too
// many" values). A non-CSL value passes through unchanged, so this is safe to apply at
// any single-value sink (assignment RHS, operator operand, condition, index value). The
// VM COLLAPSE opcode is a thin wrapper. See dev-docs/memory/csl_first_class.md.
Value collapseCsl(Value v);

// ── Inline template bodies ──────────────────────────────────────
// Kept in the header because they're tiny — each one is a single
// thunk into Value::makeContiguous. The compile-time cost per TU is
// negligible compared to dragging value.cpp into every includer.

template<class T>
inline Value::Value(Span<const T> data, std::pmr::memory_resource *mr)
    : Value()
{
    constexpr ValueType vt = detail::value_type_for<T>::type;
    *this = makeContiguous(data.data(), data.size(), vt, sizeof(T), mr);
}

template<class T>
inline Value::Value(const std::vector<T> &v, std::pmr::memory_resource *mr)
    : Value()
{
    constexpr ValueType vt = detail::value_type_for<T>::type;
    *this = makeContiguous(v.data(), v.size(), vt, sizeof(T), mr);
}

template<class T, std::size_t N>
inline Value::Value(const std::array<T, N> &a, std::pmr::memory_resource *mr)
    : Value()
{
    constexpr ValueType vt = detail::value_type_for<T>::type;
    *this = makeContiguous(a.data(), N, vt, sizeof(T), mr);
}

} // namespace numkit