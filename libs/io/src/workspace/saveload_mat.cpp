// libs/io/src/workspace/saveload_mat.cpp
//
// matio-backed save / load for MATLAB v5 .mat files. v7.3 (HDF5) is
// intentionally not linked, so attempts to use -v7.3 throw.
//
// matio's C API uses fopen() and only works with a real OS path, so for
// non-native VFS backends we stage through a temp file in the OS temp
// area: write the .mat there, then bytes ↔ VFS via the existing
// VirtualFS::readFile / writeFile string interface.
//
// Supported element types:
//   DOUBLE / SINGLE / COMPLEX (interleaved → split Re/Im for matio)
//   INT8..INT64, UINT8..UINT64
//   LOGICAL (stored as UINT8 + MAT_F_LOGICAL flag, matching MATLAB)
//   CHAR    (written as MAT_T_UTF8; UTF-16 read paths converted on load)
//   STRING  (single → CHAR row; array → CELL of CHAR rows)
//   CELL    (recursive)
//   STRUCT  (recursive, struct arrays included)
//   FUNC_HANDLE / EMPTY → empty 0×0 double placeholder
//
// Sparse, Object, Opaque on load → empty placeholder + warning text.

#include <matio.h>

#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/fs/vfs.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::io {

namespace {

// ── matio class/data-type lookup for our ValueTypes ────────────────────

struct MatTypes
{
    matio_classes cls;
    matio_types tp;
};

MatTypes matTypesFor(ValueType t)
{
    switch (t) {
        case ValueType::DOUBLE:  return {MAT_C_DOUBLE, MAT_T_DOUBLE};
        case ValueType::SINGLE:  return {MAT_C_SINGLE, MAT_T_SINGLE};
        case ValueType::INT8:    return {MAT_C_INT8,   MAT_T_INT8};
        case ValueType::INT16:   return {MAT_C_INT16,  MAT_T_INT16};
        case ValueType::INT32:   return {MAT_C_INT32,  MAT_T_INT32};
        case ValueType::INT64:   return {MAT_C_INT64,  MAT_T_INT64};
        case ValueType::UINT8:   return {MAT_C_UINT8,  MAT_T_UINT8};
        case ValueType::UINT16:  return {MAT_C_UINT16, MAT_T_UINT16};
        case ValueType::UINT32:  return {MAT_C_UINT32, MAT_T_UINT32};
        case ValueType::UINT64:  return {MAT_C_UINT64, MAT_T_UINT64};
        case ValueType::LOGICAL: return {MAT_C_UINT8,  MAT_T_UINT8};
        case ValueType::CHAR:    return {MAT_C_CHAR,   MAT_T_UTF8};
        case ValueType::COMPLEX: return {MAT_C_DOUBLE, MAT_T_DOUBLE};
        default:                 return {MAT_C_DOUBLE, MAT_T_DOUBLE};
    }
}

// MATLAB shapes are always rank ≥ 2. Pull the Value's dims into a
// rank-2-minimum size_t vector for matio.
std::vector<size_t> dimsVec(const Value &v)
{
    const Dims &d = v.dims();
    int nd = d.ndims();
    if (nd < 2) nd = 2;
    std::vector<size_t> out(nd, 1);
    for (int i = 0; i < nd; ++i)
        out[i] = d.dim(i);
    return out;
}

// Forward decl — value↔matvar conversions are mutually recursive
// (cells and structs hold child Values / matvars).
matvar_t *valueToMat(const std::string &name, const Value &v);
Value     matToValue(matvar_t *mv, std::pmr::memory_resource *mr);

// ── Value → matvar_t ───────────────────────────────────────────────────

matvar_t *makeEmpty(const std::string &name)
{
    size_t ed[2] = {0, 0};
    return Mat_VarCreate(name.empty() ? nullptr : name.c_str(),
                         MAT_C_DOUBLE, MAT_T_DOUBLE, 2, ed, nullptr, 0);
}

matvar_t *valueToMat(const std::string &name, const Value &v)
{
    const char *cname = name.empty() ? nullptr : name.c_str();
    ValueType vt = v.type();
    auto d = dimsVec(v);
    int rank = static_cast<int>(d.size());

    // EMPTY / FUNC_HANDLE → 0×0 double (no native v5 encoding).
    if (vt == ValueType::EMPTY || vt == ValueType::FUNC_HANDLE)
        return makeEmpty(name);

    // CELL — recurse per element.
    if (vt == ValueType::CELL) {
        matvar_t *cell = Mat_VarCreate(cname, MAT_C_CELL, MAT_T_CELL,
                                       rank, d.data(), nullptr, 0);
        if (!cell) return nullptr;
        size_t n = v.numel();
        for (size_t i = 0; i < n; ++i) {
            matvar_t *child = valueToMat({}, v.cellAt(i));
            if (!child) child = makeEmpty({});
            matvar_t *old = Mat_VarSetCell(cell, static_cast<int>(i), child);
            if (old) Mat_VarFree(old);
        }
        return cell;
    }

    // STRUCT — uniform field set across struct array elements.
    if (vt == ValueType::STRUCT) {
        auto names = v.fieldNamesInOrder();
        std::vector<const char *> cnames;
        cnames.reserve(names.size());
        for (auto &s : names) cnames.push_back(s.c_str());
        matvar_t *st = Mat_VarCreateStruct(
            cname, rank, d.data(),
            cnames.empty() ? nullptr : cnames.data(),
            static_cast<unsigned>(cnames.size()));
        if (!st) return nullptr;
        size_t n = v.numel();
        for (size_t i = 0; i < n; ++i) {
            const auto &fmap = v.structArrayElem(i);
            for (const auto &fn : names) {
                auto it = fmap.find(fn);
                matvar_t *child = (it != fmap.end())
                    ? valueToMat({}, it->second)
                    : makeEmpty({});
                if (!child) child = makeEmpty({});
                matvar_t *old = Mat_VarSetStructFieldByName(
                    st, fn.c_str(), i, child);
                if (old) Mat_VarFree(old);
            }
        }
        return st;
    }

    // STRING — single element → CHAR row; array → CELL of CHAR rows.
    if (vt == ValueType::STRING) {
        size_t n = v.numel();
        if (n == 1) {
            const std::string &s = v.stringElem(0);
            size_t cd[2] = {1, s.size()};
            return Mat_VarCreate(cname, MAT_C_CHAR, MAT_T_UTF8,
                                 2, cd,
                                 const_cast<char *>(s.data()), 0);
        }
        matvar_t *cell = Mat_VarCreate(cname, MAT_C_CELL, MAT_T_CELL,
                                       rank, d.data(), nullptr, 0);
        for (size_t i = 0; i < n; ++i) {
            const std::string &s = v.stringElem(i);
            size_t cd[2] = {1, s.size()};
            matvar_t *child = Mat_VarCreate(
                nullptr, MAT_C_CHAR, MAT_T_UTF8, 2, cd,
                const_cast<char *>(s.data()), 0);
            matvar_t *old = Mat_VarSetCell(cell, static_cast<int>(i), child);
            if (old) Mat_VarFree(old);
        }
        return cell;
    }

    MatTypes mt = matTypesFor(vt);

    // COMPLEX — convert interleaved {re,im} → split Re/Im buffers as
    // matio expects. Storage lives on the stack/heap of this function;
    // Mat_VarCreate with opt=0 copies the data into the matvar.
    if (vt == ValueType::COMPLEX) {
        size_t n = v.numel();
        std::vector<double> re(n), im(n);
        const Complex *src = v.complexData();
        for (size_t i = 0; i < n; ++i) {
            re[i] = src[i].real();
            im[i] = src[i].imag();
        }
        mat_complex_split_t split{re.data(), im.data()};
        return Mat_VarCreate(cname, mt.cls, mt.tp, rank, d.data(),
                             &split, MAT_F_COMPLEX);
    }

    // Real numeric / LOGICAL / CHAR — pass column-major buffer as-is.
    int flags = (vt == ValueType::LOGICAL) ? MAT_F_LOGICAL : 0;
    return Mat_VarCreate(cname, mt.cls, mt.tp, rank, d.data(),
                         const_cast<void *>(v.rawData()), flags);
}

// ── matvar_t → Value ───────────────────────────────────────────────────

Value matToValue(matvar_t *mv, std::pmr::memory_resource *mr)
{
    if (!mv) return Value();

    int rank = mv->rank;
    if (rank < 2) rank = 2;
    std::vector<size_t> d(rank, 1);
    for (int i = 0; i < mv->rank; ++i)
        d[i] = mv->dims[i];

    bool anyZero = false;
    for (int i = 0; i < mv->rank; ++i)
        if (mv->dims[i] == 0) { anyZero = true; break; }

    // CELL
    if (mv->class_type == MAT_C_CELL) {
        Value out = Value::cellND(d.data(), rank, mr);
        size_t n = out.numel();
        for (size_t i = 0; i < n; ++i) {
            matvar_t *child = Mat_VarGetCell(mv, static_cast<int>(i));
            out.cellAt(i) = matToValue(child, mr);
        }
        return out;
    }

    // STRUCT
    if (mv->class_type == MAT_C_STRUCT) {
        size_t numel = 1;
        for (int i = 0; i < mv->rank; ++i) numel *= mv->dims[i];
        unsigned nfields = Mat_VarGetNumberOfFields(mv);
        char *const *fnames = Mat_VarGetStructFieldnames(mv);
        Value out = (numel <= 1)
            ? Value::structure(mr)
            : Value::structArray(d[0], d[1], mr);
        for (size_t e = 0; e < numel; ++e) {
            for (unsigned f = 0; f < nfields; ++f) {
                matvar_t *fv = Mat_VarGetStructFieldByIndex(mv, f, e);
                Value child = matToValue(fv, mr);
                out.setField(e, fnames[f], child);
            }
        }
        return out;
    }

    // CHAR — disk may be MAT_T_UTF8 (1 byte) or MAT_T_UINT16 / UTF16.
    if (mv->class_type == MAT_C_CHAR) {
        Value out = (rank == 2)
            ? Value::matrix(d[0], d[1], ValueType::CHAR, mr)
            : Value::matrixND(d.data(), rank, ValueType::CHAR, mr);
        size_t n = out.numel();
        if (mv->data && n > 0 && !anyZero) {
            char *dst = out.charDataMut();
            if (mv->data_type == MAT_T_UINT16 || mv->data_type == MAT_T_UTF16) {
                const uint16_t *src = static_cast<const uint16_t *>(mv->data);
                for (size_t i = 0; i < n; ++i)
                    dst[i] = (src[i] < 256) ? static_cast<char>(src[i]) : '?';
            } else {
                std::memcpy(dst, mv->data, n);
            }
        }
        return out;
    }

    // Sparse / Object / Opaque / Function — unsupported in v5 mapping.
    if (mv->class_type == MAT_C_SPARSE
        || mv->class_type == MAT_C_OBJECT
        || mv->class_type == MAT_C_OPAQUE
        || mv->class_type == MAT_C_FUNCTION
        || mv->class_type == MAT_C_EMPTY) {
        return Value();
    }

    // Numeric class → our ValueType. Logical flag wins over the
    // physical class (MATLAB stores logicals as MAT_C_UINT8 + flag).
    ValueType vt;
    if (mv->isLogical) {
        vt = ValueType::LOGICAL;
    } else {
        switch (mv->class_type) {
            case MAT_C_DOUBLE: vt = ValueType::DOUBLE; break;
            case MAT_C_SINGLE: vt = ValueType::SINGLE; break;
            case MAT_C_INT8:   vt = ValueType::INT8;   break;
            case MAT_C_INT16:  vt = ValueType::INT16;  break;
            case MAT_C_INT32:  vt = ValueType::INT32;  break;
            case MAT_C_INT64:  vt = ValueType::INT64;  break;
            case MAT_C_UINT8:  vt = ValueType::UINT8;  break;
            case MAT_C_UINT16: vt = ValueType::UINT16; break;
            case MAT_C_UINT32: vt = ValueType::UINT32; break;
            case MAT_C_UINT64: vt = ValueType::UINT64; break;
            default:           vt = ValueType::DOUBLE; break;
        }
    }

    // Complex (real classes only — matio split Re/Im, single OR double).
    if (mv->isComplex
        && (mv->class_type == MAT_C_DOUBLE || mv->class_type == MAT_C_SINGLE)) {
        Value out = (rank == 2)
            ? Value::matrix(d[0], d[1], ValueType::COMPLEX, mr)
            : Value::matrixND(d.data(), rank, ValueType::COMPLEX, mr);
        size_t n = out.numel();
        if (mv->data && n > 0 && !anyZero) {
            const auto *split = static_cast<const mat_complex_split_t *>(mv->data);
            Complex *dst = out.complexDataMut();
            if (mv->class_type == MAT_C_SINGLE) {
                const float *re = static_cast<const float *>(split->Re);
                const float *im = static_cast<const float *>(split->Im);
                for (size_t i = 0; i < n; ++i) dst[i] = {re[i], im[i]};
            } else {
                const double *re = static_cast<const double *>(split->Re);
                const double *im = static_cast<const double *>(split->Im);
                for (size_t i = 0; i < n; ++i) dst[i] = {re[i], im[i]};
            }
        }
        return out;
    }

    // Plain real / logical / integer — copy raw column-major bytes.
    Value out = (rank == 2)
        ? Value::matrix(d[0], d[1], vt, mr)
        : Value::matrixND(d.data(), rank, vt, mr);
    size_t n = out.numel();
    if (mv->data && n > 0 && !anyZero) {
        size_t bytes = n * elementSize(vt);
        std::memcpy(out.rawDataMut(), mv->data, bytes);
    }
    return out;
}

// ── Native-path staging for non-NativeFS callers ───────────────────────
//
// matio uses fopen() — needs a real OS path. When the resolved VFS is
// not the native one (e.g. a CallbackFS in the IDE), stage through the
// OS temp directory: load reads VFS bytes into a temp .mat, save writes
// a temp .mat and then ships the bytes back into the VFS.

std::string makeTempMatPath()
{
    std::error_code ec;
    auto tmp = std::filesystem::temp_directory_path(ec);
    if (ec || tmp.empty()) tmp = std::filesystem::path(".");
    std::random_device rd;
    std::string suffix = std::to_string(rd()) + "_" + std::to_string(rd());
    return (tmp / ("numkit_" + suffix + ".mat")).string();
}

bool isNativeFs(VirtualFS *fs)
{
    return fs && fs->name() == "native";
}

std::string readWholeFile(const std::string &osPath)
{
    std::ifstream f(osPath, std::ios::binary);
    if (!f) throw Error("save: cannot reopen staged file '" + osPath + "'");
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

void writeWholeFile(const std::string &osPath, const std::string &bytes)
{
    std::ofstream f(osPath, std::ios::binary);
    if (!f) throw Error("load: cannot stage to '" + osPath + "'");
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!f) throw Error("load: stage write to '" + osPath + "' failed");
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public entry points (declared at the top of saveload.cpp for dispatch).
// ════════════════════════════════════════════════════════════════════════

void saveMat(Engine &engine, Environment &env,
             const std::string &filename,
             const std::vector<std::string> &varnames,
             int matVersion)
{
    // Map MATLAB-flag version (4/5/7) → matio file type + compression.
    // `-v7` is a v5 container with zlib compression, matching MATLAB's
    // convention (full v7.3 is HDF5 and intentionally not linked).
    mat_ft       fileType    = MAT_FT_MAT5;
    matio_compression compression = MAT_COMPRESSION_NONE;
    switch (matVersion) {
        case 4: fileType = MAT_FT_MAT4; compression = MAT_COMPRESSION_NONE; break;
        case 5:
        case 6: fileType = MAT_FT_MAT5; compression = MAT_COMPRESSION_NONE; break;
        case 7: fileType = MAT_FT_MAT5; compression = MAT_COMPRESSION_ZLIB; break;
        default:
            throw Error("save: unsupported MAT version (only -v4 / -v6 / -v7)");
    }

    auto resolved = engine.resolvePath(filename);
    if (!resolved.fs)
        throw Error("save: cannot resolve '" + filename + "'");

    bool stage = !isNativeFs(resolved.fs);
    std::string osPath = stage ? makeTempMatPath() : resolved.path;

    mat_t *matfp = Mat_CreateVer(osPath.c_str(), nullptr, fileType);
    if (!matfp) {
        if (stage) { std::error_code ec; std::filesystem::remove(osPath, ec); }
        throw Error("save: cannot create '" + filename + "'");
    }

    // Empty varnames → save whole workspace (MATLAB default).
    std::vector<std::string> names = varnames;
    if (names.empty())
        names = env.localNames();

    try {
        for (const auto &nm : names) {
            Value *v = env.get(nm);
            if (!v)
                throw Error("save: variable '" + nm + "' not found");
            matvar_t *mv = valueToMat(nm, *v);
            if (!mv)
                throw Error("save: cannot encode variable '" + nm + "'");
            int rc = Mat_VarWrite(matfp, mv, compression);
            Mat_VarFree(mv);
            if (rc != 0)
                throw Error("save: write failed for '" + nm + "'");
        }
    } catch (...) {
        Mat_Close(matfp);
        if (stage) { std::error_code ec; std::filesystem::remove(osPath, ec); }
        throw;
    }
    Mat_Close(matfp);

    if (stage) {
        std::string bytes;
        try {
            bytes = readWholeFile(osPath);
        } catch (...) {
            std::error_code ec; std::filesystem::remove(osPath, ec);
            throw;
        }
        std::error_code ec;
        std::filesystem::remove(osPath, ec);
        try {
            resolved.fs->writeFile(resolved.path, bytes);
        } catch (const std::exception &e) {
            throw Error(std::string("save: ") + e.what());
        }
    }
}

void loadMat(Engine &engine, Environment &env,
             const std::string &filename,
             size_t nargout, Span<Value> outs)
{
    std::pmr::memory_resource *mr = engine.resource();
    auto resolved = engine.resolvePath(filename);
    if (!resolved.fs)
        throw Error("load: cannot resolve '" + filename + "'");

    bool stage = !isNativeFs(resolved.fs);
    std::string osPath = resolved.path;
    if (stage) {
        std::string bytes;
        try {
            bytes = resolved.fs->readFile(resolved.path);
        } catch (const std::exception &e) {
            throw Error(std::string("load: ") + e.what());
        }
        osPath = makeTempMatPath();
        writeWholeFile(osPath, bytes);
    }

    mat_t *matfp = Mat_Open(osPath.c_str(), MAT_ACC_RDONLY);
    if (!matfp) {
        if (stage) { std::error_code ec; std::filesystem::remove(osPath, ec); }
        throw Error("load: cannot open '" + filename + "'");
    }

    Value asStruct;
    if (nargout > 0)
        asStruct = Value::structure(mr);

    while (true) {
        matvar_t *mv = Mat_VarReadNext(matfp);
        if (!mv) break;
        std::string name = mv->name ? mv->name : "";
        Value v = matToValue(mv, mr);
        Mat_VarFree(mv);
        if (name.empty()) continue;
        if (nargout > 0)
            asStruct.setField(0, name, v);
        else
            env.set(name, std::move(v));
    }
    Mat_Close(matfp);
    if (stage) { std::error_code ec; std::filesystem::remove(osPath, ec); }

    if (nargout > 0)
        outs[0] = std::move(asStruct);
}

} // namespace numkit::io
