// runtime/src/saveload_mat.cpp
//
// In-tree autonomous zero-dependency MATLAB .mat file codec (Level 4 & Level 5).
// Supports uncompressed Level 5 (-v6), compressed Level 5 (-v7 via in-tree zlib),
// and Level 4 (-v4). Zero external libraries (matio and zlib eliminated).

#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/fs/vfs.hpp>
#include <numkit/ops/deflate.hpp>

#include <algorithm>
#include <complex>
#include <cstdint>
#include <cstring>
#include <memory_resource>
#include <string>
#include <vector>

namespace numkit::runtime {

namespace {

// ============================================================================
// MAT5 Constants & Tags
// ============================================================================

enum Mat5DataType : std::uint32_t {
    miINT8       = 1,
    miUINT8      = 2,
    miINT16      = 3,
    miUINT16     = 4,
    miINT32      = 5,
    miUINT32     = 6,
    miSINGLE     = 7,
    miDOUBLE     = 9,
    miINT64      = 12,
    miUINT64     = 13,
    miMATRIX     = 14,
    miCOMPRESSED = 15,
    miUTF8       = 16,
    miUTF16      = 17,
    miUTF32      = 18
};

enum Mat5ArrayClass : std::uint8_t {
    mxCELL_CLASS    = 1,
    mxSTRUCT_CLASS  = 2,
    mxOBJECT_CLASS  = 3,
    mxCHAR_CLASS    = 4,
    mxSPARSE_CLASS  = 5,
    mxDOUBLE_CLASS  = 6,
    mxSINGLE_CLASS  = 7,
    mxINT8_CLASS    = 8,
    mxUINT8_CLASS   = 9,
    mxINT16_CLASS   = 10,
    mxUINT16_CLASS  = 11,
    mxINT32_CLASS   = 12,
    mxUINT32_CLASS  = 13,
    mxINT64_CLASS   = 14,
    mxUINT64_CLASS  = 15
};

constexpr std::uint8_t MAT5_FLAG_LOGICAL = 0x02;
constexpr std::uint8_t MAT5_FLAG_GLOBAL  = 0x04;
constexpr std::uint8_t MAT5_FLAG_COMPLEX = 0x08;

// Helper: Factory for matrix of arbitrary shape
Value createMatrix(const Dims &dims, ValueType vt, std::pmr::memory_resource *mr) {
    if (dims.ndims() <= 2) {
        return Value::matrix(dims.rows(), dims.cols(), vt, mr);
    } else if (dims.ndims() == 3) {
        return Value::matrix3d(dims.dim(0), dims.dim(1), dims.dim(2), vt, mr);
    } else {
        std::vector<size_t> d(dims.ndims());
        for (int i = 0; i < dims.ndims(); ++i) d[i] = dims.dim(i);
        return Value::matrixND(d.data(), dims.ndims(), vt, mr);
    }
}

Value createComplexMatrix(const Dims &dims, std::pmr::memory_resource *mr) {
    if (dims.ndims() <= 2) {
        return Value::complexMatrix(dims.rows(), dims.cols(), mr);
    } else {
        Value v = createMatrix(dims, ValueType::DOUBLE, mr);
        v.promoteToComplex(mr);
        return v;
    }
}

Value createCell(const Dims &dims, std::pmr::memory_resource *mr) {
    if (dims.ndims() <= 2) {
        return Value::cell(dims.rows(), dims.cols(), mr);
    } else if (dims.ndims() == 3) {
        return Value::cell3D(dims.dim(0), dims.dim(1), dims.dim(2), mr);
    } else {
        std::vector<size_t> d(dims.ndims());
        for (int i = 0; i < dims.ndims(); ++i) d[i] = dims.dim(i);
        return Value::cellND(d.data(), dims.ndims(), mr);
    }
}

// Helper: Byte stream writer
class MatWriter {
public:
    std::vector<std::uint8_t> bytes;

    void writeU8(std::uint8_t v) { bytes.push_back(v); }

    void writeU16(std::uint16_t v) {
        bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }

    void writeU32(std::uint32_t v) {
        bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void writeBytes(const void *data, std::size_t count) {
        if (count == 0 || !data) return;
        const auto *p = reinterpret_cast<const std::uint8_t*>(data);
        bytes.insert(bytes.end(), p, p + count);
    }

    void writeTag(std::uint32_t type, const void *data, std::uint32_t byteCount) {
        if (byteCount <= 4 && byteCount > 0) {
            // Small Data Element Format (SDEF): [byteCount:16, type:16], [data:32]
            std::uint32_t tag = ((byteCount & 0xFFFF) << 16) | (type & 0xFFFF);
            writeU32(tag);
            std::uint32_t val = 0;
            if (data) {
                std::memcpy(&val, data, byteCount);
            }
            writeU32(val);
        } else {
            writeU32(type);
            writeU32(byteCount);
            if (byteCount > 0 && data) {
                writeBytes(data, byteCount);
            }
            std::size_t pad = ((byteCount + 7) & ~7) - byteCount;
            for (std::size_t i = 0; i < pad; ++i) writeU8(0);
        }
    }
};

// Helper: Byte stream reader
class MatReader {
public:
    const std::uint8_t *data;
    std::size_t len;
    std::size_t pos = 0;

    MatReader(const std::uint8_t *d, std::size_t l) : data(d), len(l), pos(0) {}

    bool eof() const { return pos >= len; }
    std::size_t remaining() const { return (pos <= len) ? (len - pos) : 0; }

    std::uint8_t readU8() {
        if (pos + 1 > len) throw Error("load: truncated MAT file");
        return data[pos++];
    }

    std::uint16_t readU16() {
        if (pos + 2 > len) throw Error("load: truncated MAT file");
        std::uint16_t v = static_cast<std::uint16_t>(data[pos]) |
                          (static_cast<std::uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        return v;
    }

    std::uint32_t readU32() {
        if (pos + 4 > len) throw Error("load: truncated MAT file");
        std::uint32_t v = static_cast<std::uint32_t>(data[pos]) |
                          (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
                          (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
                          (static_cast<std::uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    }

    struct Tag {
        std::uint32_t type;
        std::uint32_t byteCount;
        const std::uint8_t *payload;
        std::size_t totalAdvance;
    };

    Tag readTag() {
        if (pos + 8 > len) throw Error("load: truncated MAT tag");
        std::uint32_t tag0 = static_cast<std::uint32_t>(data[pos]) |
                             (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
                             (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
                             (static_cast<std::uint32_t>(data[pos + 3]) << 24);
        
        Tag t;
        if ((tag0 >> 16) != 0) {
            // Small Data Element Format (SDEF)
            t.type = tag0 & 0xFFFF;
            t.byteCount = (tag0 >> 16) & 0xFFFF;
            t.payload = data + pos + 4;
            t.totalAdvance = 8;
            pos += 8;
        } else {
            // Standard format
            t.type = tag0;
            t.byteCount = static_cast<std::uint32_t>(data[pos + 4]) |
                          (static_cast<std::uint32_t>(data[pos + 5]) << 8) |
                          (static_cast<std::uint32_t>(data[pos + 6]) << 16) |
                          (static_cast<std::uint32_t>(data[pos + 7]) << 24);
            t.payload = data + pos + 8;
            std::size_t padded = (t.byteCount + 7) & ~7;
            if (pos + 8 + padded > len) {
                if (pos + 8 + t.byteCount > len)
                    throw Error("load: corrupted MAT element length");
                padded = t.byteCount;
            }
            t.totalAdvance = 8 + padded;
            pos += t.totalAdvance;
        }
        return t;
    }
};

// Forward declarations for recursive encoding / decoding
void encodeMat5Matrix(MatWriter &mw, const std::string &name, const Value &v);
std::pair<std::string, Value> decodeMat5Matrix(MatReader &mr, std::pmr::memory_resource *mem);

// ============================================================================
// MAT5 Serialization (Value -> Bytes)
// ============================================================================

void encodeMat5Matrix(MatWriter &mw, const std::string &name, const Value &v) {
    MatWriter inner;

    // 1. Array Flags
    std::uint8_t arrayClass = mxDOUBLE_CLASS;
    std::uint8_t flags = 0;

    switch (v.type()) {
        case ValueType::DOUBLE:  arrayClass = mxDOUBLE_CLASS; break;
        case ValueType::SINGLE:  arrayClass = mxSINGLE_CLASS; break;
        case ValueType::INT8:    arrayClass = mxINT8_CLASS;   break;
        case ValueType::UINT8:   arrayClass = mxUINT8_CLASS;  break;
        case ValueType::INT16:   arrayClass = mxINT16_CLASS;  break;
        case ValueType::UINT16:  arrayClass = mxUINT16_CLASS; break;
        case ValueType::INT32:   arrayClass = mxINT32_CLASS;  break;
        case ValueType::UINT32:  arrayClass = mxUINT32_CLASS; break;
        case ValueType::INT64:   arrayClass = mxINT64_CLASS;  break;
        case ValueType::UINT64:  arrayClass = mxUINT64_CLASS; break;
        case ValueType::LOGICAL: arrayClass = mxUINT8_CLASS; flags |= MAT5_FLAG_LOGICAL; break;
        case ValueType::CHAR:    arrayClass = mxCHAR_CLASS;   break;
        case ValueType::COMPLEX: arrayClass = mxDOUBLE_CLASS; flags |= MAT5_FLAG_COMPLEX; break;
        case ValueType::CELL:    arrayClass = mxCELL_CLASS;   break;
        case ValueType::STRUCT:  arrayClass = mxSTRUCT_CLASS; break;
        case ValueType::OBJECT:  arrayClass = mxOBJECT_CLASS; break;
        case ValueType::FUNC_HANDLE: arrayClass = mxDOUBLE_CLASS; break;
        default:                 arrayClass = mxDOUBLE_CLASS; break;
    }

    std::uint32_t flagsBuf[2] = {
        static_cast<std::uint32_t>(arrayClass | (flags << 8)),
        0
    };
    inner.writeTag(miUINT32, flagsBuf, 8);

    // 2. Dimensions Array (at least 2 dims [rows, cols])
    const Dims &d = v.dims();
    int nd = d.ndims();
    if (nd < 2) nd = 2;
    std::vector<std::int32_t> dimsVec(nd, 1);
    for (int i = 0; i < nd; ++i) {
        dimsVec[i] = static_cast<std::int32_t>(d.dim(i));
    }
    if (v.isFuncHandle()) {
        // v5 .mat emits 0x0 double placeholder for function handle
        dimsVec = {0, 0};
    }
    inner.writeTag(miINT32, dimsVec.data(), static_cast<std::uint32_t>(dimsVec.size() * sizeof(std::int32_t)));

    // 3. Array Name
    inner.writeTag(miINT8, name.data(), static_cast<std::uint32_t>(name.size()));

    // 4. Data Sub-elements
    std::size_t numElems = v.isFuncHandle() ? 0 : d.numel();

    if (v.type() == ValueType::CELL) {
        // Cell elements written as full child miMATRIX elements (column-major)
        for (std::size_t i = 0; i < numElems; ++i) {
            const Value &elem = v.cellAt(i);
            encodeMat5Matrix(inner, "", elem);
        }
    } else if (v.type() == ValueType::STRUCT || v.type() == ValueType::OBJECT) {
        std::vector<std::string> fnames;
        if (v.type() == ValueType::OBJECT) {
            std::string className = v.objectClassName();
            inner.writeTag(miINT8, className.data(), static_cast<std::uint32_t>(className.size()));
            if (const auto *st = v.objectStateConst()) {
                for (const auto &[k, _] : st->props) {
                    fnames.push_back(k);
                }
            }
        } else {
            fnames = v.fieldNamesInOrder();
        }

        std::int32_t maxLen = 32;
        inner.writeTag(miINT32, &maxLen, sizeof(std::int32_t));

        std::vector<char> nameTable(fnames.size() * 32, 0);
        for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
            std::strncpy(nameTable.data() + fi * 32, fnames[fi].c_str(), 31);
        }
        inner.writeTag(miINT8, nameTable.data(), static_cast<std::uint32_t>(nameTable.size()));

        // Field values: for each field, for each element in struct / object array
        for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
            for (std::size_t ei = 0; ei < numElems; ++ei) {
                Value fval;
                if (v.type() == ValueType::OBJECT) {
                    const auto *st = v.objectStateAt(ei);
                    if (st) {
                        auto it = st->props.find(fnames[fi]);
                        if (it != st->props.end()) fval = it->second;
                    }
                } else if (v.isStructArray()) {
                    auto &m = const_cast<Value&>(v).structArrayElem(ei);
                    auto it = m.find(fnames[fi]);
                    if (it != m.end()) fval = it->second;
                } else {
                    auto &m = const_cast<Value&>(v).structFields();
                    auto it = m.find(fnames[fi]);
                    if (it != m.end()) fval = it->second;
                }
                encodeMat5Matrix(inner, "", fval);
            }
        }
    } else if (v.type() == ValueType::CHAR) {
        // UTF-16 character codes
        const char *src = v.charData();
        std::vector<std::uint16_t> utf16(numElems);
        for (std::size_t i = 0; i < numElems; ++i) {
            utf16[i] = static_cast<std::uint8_t>(src[i]);
        }
        inner.writeTag(miUINT16, utf16.data(), static_cast<std::uint32_t>(numElems * sizeof(std::uint16_t)));
    } else if (v.type() == ValueType::COMPLEX) {
        // Split complex into real and imaginary arrays
        std::vector<double> realPart(numElems);
        std::vector<double> imagPart(numElems);
        const auto *cdata = v.complexData();
        for (std::size_t i = 0; i < numElems; ++i) {
            realPart[i] = cdata[i].real();
            imagPart[i] = cdata[i].imag();
        }
        inner.writeTag(miDOUBLE, realPart.data(), static_cast<std::uint32_t>(numElems * sizeof(double)));
        inner.writeTag(miDOUBLE, imagPart.data(), static_cast<std::uint32_t>(numElems * sizeof(double)));
    } else if (v.isFuncHandle() || numElems == 0) {
        inner.writeTag(miDOUBLE, nullptr, 0);
    } else {
        // Numeric and Logical arrays
        Mat5DataType dt = miDOUBLE;
        std::size_t elemSize = sizeof(double);

        switch (v.type()) {
            case ValueType::DOUBLE:  dt = miDOUBLE; elemSize = 8; break;
            case ValueType::SINGLE:  dt = miSINGLE; elemSize = 4; break;
            case ValueType::INT8:    dt = miINT8;   elemSize = 1; break;
            case ValueType::UINT8:   dt = miUINT8;  elemSize = 1; break;
            case ValueType::LOGICAL: dt = miUINT8;  elemSize = 1; break;
            case ValueType::INT16:   dt = miINT16;  elemSize = 2; break;
            case ValueType::UINT16:  dt = miUINT16; elemSize = 2; break;
            case ValueType::INT32:   dt = miINT32;  elemSize = 4; break;
            case ValueType::UINT32:  dt = miUINT32; elemSize = 4; break;
            case ValueType::INT64:   dt = miINT64;  elemSize = 8; break;
            case ValueType::UINT64:  dt = miUINT64; elemSize = 8; break;
            default:                 dt = miDOUBLE; elemSize = 8; break;
        }

        const void *srcData = (v.type() == ValueType::LOGICAL) ? v.logicalData() : v.rawData();
        inner.writeTag(dt, srcData, static_cast<std::uint32_t>(numElems * elemSize));
    }

    // Write miMATRIX tag wrapping inner buffer
    mw.writeTag(miMATRIX, inner.bytes.data(), static_cast<std::uint32_t>(inner.bytes.size()));
}

// ============================================================================
// MAT5 Deserialization (Bytes -> Value)
// ============================================================================

std::pair<std::string, Value> decodeMat5Matrix(MatReader &mr, std::pmr::memory_resource *mem) {
    auto matrixTag = mr.readTag();
    if (matrixTag.type != miMATRIX) {
        throw Error("load: expected miMATRIX element in MAT5 file");
    }

    MatReader ir(matrixTag.payload, matrixTag.byteCount);

    // 1. Array Flags
    auto flagsTag = ir.readTag();
    if (flagsTag.byteCount < 4) throw Error("load: malformed array flags");
    std::uint8_t arrayClass = flagsTag.payload[0];
    std::uint8_t flags = flagsTag.payload[1];
    bool isLogical = (flags & MAT5_FLAG_LOGICAL) != 0;
    bool isComplex = (flags & MAT5_FLAG_COMPLEX) != 0;

    // 2. Dimensions Array
    auto dimsTag = ir.readTag();
    int nd = static_cast<int>(dimsTag.byteCount / sizeof(std::int32_t));
    if (nd < 2) nd = 2;
    std::vector<size_t> d(nd, 1);
    const auto *dp = reinterpret_cast<const std::int32_t*>(dimsTag.payload);
    for (int i = 0; i < nd; ++i) {
        d[i] = (i < static_cast<int>(dimsTag.byteCount / 4)) ? static_cast<size_t>(dp[i]) : 1;
    }
    Dims shape(d.data(), nd);
    std::size_t numElems = shape.numel();

    // 3. Array Name
    auto nameTag = ir.readTag();
    std::string name;
    if (nameTag.byteCount > 0 && nameTag.payload) {
        name = std::string(reinterpret_cast<const char*>(nameTag.payload), nameTag.byteCount);
        while (!name.empty() && name.back() == '\0') name.pop_back();
    }

    // 4. Data Sub-elements
    Value result;

    if (arrayClass == mxCELL_CLASS) {
        result = createCell(shape, mem);
        for (std::size_t i = 0; i < numElems; ++i) {
            auto [childName, childVal] = decodeMat5Matrix(ir, mem);
            result.cellAt(i) = childVal;
        }
    } else if (arrayClass == mxSTRUCT_CLASS || arrayClass == mxOBJECT_CLASS) {
        std::string objClassName;
        if (arrayClass == mxOBJECT_CLASS) {
            auto classNameTag = ir.readTag();
            if (classNameTag.payload && classNameTag.byteCount > 0) {
                objClassName = std::string(reinterpret_cast<const char*>(classNameTag.payload), classNameTag.byteCount);
            }
        }

        auto fnamelenTag = ir.readTag();
        std::uint32_t fnLen = 32;
        if (fnamelenTag.byteCount >= 4) {
            fnLen = *reinterpret_cast<const std::uint32_t*>(fnamelenTag.payload);
        }
        if (fnLen == 0) fnLen = 32;

        auto fnamesTag = ir.readTag();
        std::size_t numFields = fnamesTag.byteCount / fnLen;
        std::vector<std::string> fnames;
        for (std::size_t fi = 0; fi < numFields; ++fi) {
            const char *fnPtr = reinterpret_cast<const char*>(fnamesTag.payload + fi * fnLen);
            std::size_t curLen = 0;
            while (curLen < fnLen && fnPtr[curLen] != '\0') ++curLen;
            fnames.emplace_back(fnPtr, curLen);
        }

        std::vector<std::vector<Value>> fieldVals(fnames.size(), std::vector<Value>(numElems));
        for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
            for (std::size_t ei = 0; ei < numElems; ++ei) {
                auto [childName, childVal] = decodeMat5Matrix(ir, mem);
                fieldVals[fi][ei] = std::move(childVal);
            }
        }

        if (arrayClass == mxSTRUCT_CLASS) {
            if (numElems <= 1) {
                result = Value::structure(mem);
            } else {
                result = Value::structArray(shape.rows(), shape.cols(), mem);
            }
            for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
                for (std::size_t ei = 0; ei < numElems; ++ei) {
                    result.setField(ei, fnames[fi], fieldVals[fi][ei]);
                }
            }
        } else {
            // mxOBJECT_CLASS
            if (numElems <= 1) {
                auto st = std::make_shared<ObjectState>(mem);
                for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
                    st->props[fnames[fi]] = std::move(fieldVals[fi][0]);
                }
                result = Value::object(objClassName, std::move(st), false, mem);
            } else {
                std::vector<std::shared_ptr<ObjectState>> states;
                states.reserve(numElems);
                for (std::size_t ei = 0; ei < numElems; ++ei) {
                    auto st = std::make_shared<ObjectState>(mem);
                    for (std::size_t fi = 0; fi < fnames.size(); ++fi) {
                        st->props[fnames[fi]] = std::move(fieldVals[fi][ei]);
                    }
                    states.push_back(std::move(st));
                }
                result = Value::objectArray(objClassName, shape, std::move(states), false, mem);
            }
        }
    } else if (arrayClass == mxCHAR_CLASS) {
        auto dataTag = ir.readTag();
        result = createMatrix(shape, ValueType::CHAR, mem);
        char *out = result.charDataMut();
        if (dataTag.type == miUINT16) {
            const auto *u16 = reinterpret_cast<const std::uint16_t*>(dataTag.payload);
            std::size_t charCount = std::min(numElems, static_cast<std::size_t>(dataTag.byteCount / 2));
            for (std::size_t i = 0; i < charCount; ++i) {
                out[i] = static_cast<char>(u16[i] & 0xFF);
            }
        } else if (dataTag.type == miUTF8 || dataTag.type == miUINT8 || dataTag.type == miINT8) {
            std::size_t charCount = std::min(numElems, static_cast<std::size_t>(dataTag.byteCount));
            if (charCount > 0 && dataTag.payload) {
                std::memcpy(out, dataTag.payload, charCount);
            }
        }
    } else {
        // Numeric and Logical arrays
        ValueType vt = ValueType::DOUBLE;
        if (isLogical) {
            vt = ValueType::LOGICAL;
        } else if (isComplex) {
            vt = ValueType::COMPLEX;
        } else {
            switch (arrayClass) {
                case mxDOUBLE_CLASS: vt = ValueType::DOUBLE; break;
                case mxSINGLE_CLASS: vt = ValueType::SINGLE; break;
                case mxINT8_CLASS:   vt = ValueType::INT8;   break;
                case mxUINT8_CLASS:  vt = ValueType::UINT8;  break;
                case mxINT16_CLASS:  vt = ValueType::INT16;  break;
                case mxUINT16_CLASS: vt = ValueType::UINT16; break;
                case mxINT32_CLASS:  vt = ValueType::INT32;  break;
                case mxUINT32_CLASS: vt = ValueType::UINT32; break;
                case mxINT64_CLASS:  vt = ValueType::INT64;  break;
                case mxUINT64_CLASS: vt = ValueType::UINT64; break;
                default:             vt = ValueType::DOUBLE; break;
            }
        }

        auto realTag = ir.readTag();

        if (isComplex) {
            auto imagTag = ir.readTag();
            result = createComplexMatrix(shape, mem);
            auto *cdata = result.complexDataMut();

            // Real
            if (realTag.type == miDOUBLE) {
                const auto *rp = reinterpret_cast<const double*>(realTag.payload);
                for (std::size_t i = 0; i < numElems; ++i) cdata[i] = std::complex<double>(rp[i], cdata[i].imag());
            } else if (realTag.type == miSINGLE) {
                const auto *rp = reinterpret_cast<const float*>(realTag.payload);
                for (std::size_t i = 0; i < numElems; ++i) cdata[i] = std::complex<double>(rp[i], cdata[i].imag());
            }
            // Imag
            if (imagTag.type == miDOUBLE) {
                const auto *ip = reinterpret_cast<const double*>(imagTag.payload);
                for (std::size_t i = 0; i < numElems; ++i) cdata[i] = std::complex<double>(cdata[i].real(), ip[i]);
            } else if (imagTag.type == miSINGLE) {
                const auto *ip = reinterpret_cast<const float*>(imagTag.payload);
                for (std::size_t i = 0; i < numElems; ++i) cdata[i] = std::complex<double>(cdata[i].real(), ip[i]);
            }
        } else {
            result = createMatrix(shape, vt, mem);
            if (numElems > 0 && realTag.payload) {
                std::size_t copyBytes = std::min<std::size_t>(result.rawBytes(), realTag.byteCount);
                std::memcpy(result.rawDataMut(), realTag.payload, copyBytes);
            }
        }
    }

    return {name, result};
}

// ============================================================================
// MAT4 Codec (-v4)
// ============================================================================

void saveMat4(const std::string &osPath,
              const std::vector<std::pair<std::string, Value>> &vars,
              VirtualFS &fs)
{
    MatWriter mw;
    for (const auto &[name, v] : vars) {
        if (!v.isNumeric() && !v.isChar() && !v.isComplex()) continue;
        const Dims &d = v.dims();
        std::int32_t rows = static_cast<std::int32_t>(d.rows());
        std::int32_t cols = static_cast<std::int32_t>(d.cols());
        std::int32_t imagf = v.isComplex() ? 1 : 0;
        std::int32_t pType = 0; // double
        if (v.type() == ValueType::SINGLE) pType = 1;
        else if (v.type() == ValueType::INT32) pType = 2;
        else if (v.type() == ValueType::INT16) pType = 3;
        else if (v.type() == ValueType::UINT16) pType = 4;
        else if (v.type() == ValueType::UINT8) pType = 5;
        std::int32_t tType = v.isChar() ? 1 : 0;
        std::int32_t mType = pType * 10 + tType;

        std::int32_t namlen = static_cast<std::int32_t>(name.size() + 1);

        mw.writeU32(static_cast<std::uint32_t>(mType));
        mw.writeU32(static_cast<std::uint32_t>(rows));
        mw.writeU32(static_cast<std::uint32_t>(cols));
        mw.writeU32(static_cast<std::uint32_t>(imagf));
        mw.writeU32(static_cast<std::uint32_t>(namlen));
        mw.writeBytes(name.c_str(), namlen);

        std::size_t numElems = static_cast<std::size_t>(rows * cols);
        if (v.isComplex()) {
            std::vector<double> realPart(numElems);
            std::vector<double> imagPart(numElems);
            const auto *cdata = v.complexData();
            for (std::size_t i = 0; i < numElems; ++i) {
                realPart[i] = cdata[i].real();
                imagPart[i] = cdata[i].imag();
            }
            mw.writeBytes(realPart.data(), numElems * sizeof(double));
            mw.writeBytes(imagPart.data(), numElems * sizeof(double));
        } else {
            mw.writeBytes(v.rawData(), v.rawBytes());
        }
    }
    // Binary channel: MAT bytes contain values ≥ 0x80 that the text
    // writeFile path would UTF-8-mangle crossing the JS↔WASM boundary.
    fs.writeFileBytes(osPath, std::string(reinterpret_cast<const char*>(mw.bytes.data()), mw.bytes.size()));
}

void loadMat4(const std::string &osPath,
              VirtualFS &fs,
              Environment &env,
              size_t nargout,
              Span<Value> outs,
              std::pmr::memory_resource *mr)
{
    std::string bytes = fs.readFileBytes(osPath);
    const auto *p = reinterpret_cast<const std::uint8_t*>(bytes.data());
    std::size_t len = bytes.size();
    std::size_t pos = 0;

    Value asStruct;
    if (nargout > 0) asStruct = Value::structure(mr);
    std::size_t loadedCount = 0;

    while (pos + 20 <= len) {
        std::int32_t mType = *reinterpret_cast<const std::int32_t*>(p + pos);
        std::int32_t rows  = *reinterpret_cast<const std::int32_t*>(p + pos + 4);
        std::int32_t cols  = *reinterpret_cast<const std::int32_t*>(p + pos + 8);
        std::int32_t imagf = *reinterpret_cast<const std::int32_t*>(p + pos + 12);
        std::int32_t namlen= *reinterpret_cast<const std::int32_t*>(p + pos + 16);
        pos += 20;

        // Validation for MAT4 header
        int mEndian = mType / 1000;
        int mFormat = (mType / 100) % 10;
        int pType = (mType / 10) % 10;
        int tType = mType % 10;

        if (mEndian < 0 || mEndian > 4 || mFormat != 0 || pType < 0 || pType > 5 || tType < 0 || tType > 2) {
            throw Error("load: invalid or corrupted .mat file");
        }
        if (rows < 0 || cols < 0 || (imagf != 0 && imagf != 1) || namlen <= 0 || namlen > 256) {
            throw Error("load: invalid or corrupted .mat file");
        }

        if (pos + namlen > len) throw Error("load: truncated MAT4 file");
        std::string name(reinterpret_cast<const char*>(p + pos));
        pos += namlen;

        std::size_t numElems = static_cast<std::size_t>(rows * cols);
        std::size_t elemSize = sizeof(double);
        ValueType vt = ValueType::DOUBLE;
        if (pType == 1) { vt = ValueType::SINGLE; elemSize = 4; }
        else if (pType == 2) { vt = ValueType::INT32; elemSize = 4; }
        else if (pType == 3) { vt = ValueType::INT16; elemSize = 2; }
        else if (pType == 4) { vt = ValueType::UINT16; elemSize = 2; }
        else if (pType == 5) { vt = ValueType::UINT8; elemSize = 1; }

        if (tType == 1) vt = ValueType::CHAR;

        Value v;
        if (imagf == 1) {
            v = Value::complexMatrix(rows, cols, mr);
            auto *cdata = v.complexDataMut();
            std::size_t realBytes = numElems * sizeof(double);
            if (pos + realBytes * 2 > len) throw Error("load: truncated MAT4 complex data");
            const auto *rp = reinterpret_cast<const double*>(p + pos);
            const auto *ip = reinterpret_cast<const double*>(p + pos + realBytes);
            for (std::size_t i = 0; i < numElems; ++i) {
                cdata[i] = std::complex<double>(rp[i], ip[i]);
            }
            pos += realBytes * 2;
        } else {
            v = Value::matrix(rows, cols, vt, mr);
            std::size_t dataBytes = numElems * elemSize;
            if (pos + dataBytes > len) throw Error("load: truncated MAT4 data");
            if (dataBytes > 0) {
                std::memcpy(v.rawDataMut(), p + pos, dataBytes);
                pos += dataBytes;
            }
        }

        if (!name.empty()) {
            if (nargout > 0) asStruct.setField(0, name, v);
            else env.set(name, std::move(v));
            ++loadedCount;
        }
    }

    if (loadedCount == 0 && len > 0) {
        throw Error("load: invalid or corrupted .mat file");
    }

    if (nargout > 0) outs[0] = std::move(asStruct);
}

} // anonymous namespace

// ============================================================================
// Public saveMat & loadMat API
// ============================================================================

void saveMat(Engine &engine, Environment &env,
             const std::string &filename,
             const std::vector<std::string> &varnames,
             int matVersion)
{
    auto resolved = engine.resolvePath(filename);
    if (!resolved.fs)
        throw Error("save: cannot resolve '" + filename + "'");

    std::vector<std::string> names = varnames;
    if (names.empty()) names = env.localNames();

    std::vector<std::pair<std::string, Value>> vars;
    for (const auto &nm : names) {
        Value *v = env.get(nm);
        if (!v) throw Error("save: variable '" + nm + "' not found");
        vars.emplace_back(nm, *v);
    }

    if (matVersion == 4) {
        saveMat4(resolved.path, vars, *resolved.fs);
        return;
    }

    // Level 5 Header (128 bytes)
    MatWriter mw;
    std::string hdrText = "MATLAB 5.0 MAT-file, Platform: NumKit Autonomous Engine, Created by NumKit";
    hdrText.resize(116, ' ');
    mw.writeBytes(hdrText.data(), 116);
    std::uint8_t zeroSubsystem[8] = {0};
    mw.writeBytes(zeroSubsystem, 8);
    mw.writeU16(0x0100); // Version 1.0
    mw.writeU8('I');     // Little-endian
    mw.writeU8('M');

    // Encode variables
    for (const auto &[nm, v] : vars) {
        MatWriter itemWriter;
        encodeMat5Matrix(itemWriter, nm, v);

        if (matVersion == 7) {
            // -v7: Compressed miMATRIX via zlib
            std::vector<std::uint8_t> comp = ops::zlibCompress(
                itemWriter.bytes.data(), itemWriter.bytes.size(), 6);
            mw.writeTag(miCOMPRESSED, comp.data(), static_cast<std::uint32_t>(comp.size()));
        } else {
            // -v6 / -v5: Uncompressed miMATRIX
            mw.writeBytes(itemWriter.bytes.data(), itemWriter.bytes.size());
        }
    }

    // Binary channel — see the writeFileBytes note in saveMat4 above.
    resolved.fs->writeFileBytes(resolved.path, std::string(reinterpret_cast<const char*>(mw.bytes.data()), mw.bytes.size()));
}

void loadMat(Engine &engine, Environment &env,
             const std::string &filename,
             size_t nargout, Span<Value> outs)
{
    std::pmr::memory_resource *mr = engine.resource();
    auto resolved = engine.resolvePath(filename);
    if (!resolved.fs)
        throw Error("load: cannot resolve '" + filename + "'");

    std::string bytes;
    try {
        bytes = resolved.fs->readFileBytes(resolved.path);
    } catch (const std::exception &e) {
        throw Error(std::string("load: ") + e.what());
    }

    if (bytes.size() < 16)
        throw Error("load: file too short or empty");

    // Detect MAT5 vs MAT4
    // MAT5 header: bytes 126..127 == 'IM' or 'MI'
    if (bytes.size() >= 128 && ((bytes[126] == 'I' && bytes[127] == 'M') ||
                               (bytes[126] == 'M' && bytes[127] == 'I'))) {
        // MAT5
        MatReader mrReader(reinterpret_cast<const std::uint8_t*>(bytes.data() + 128), bytes.size() - 128);

        Value asStruct;
        if (nargout > 0) asStruct = Value::structure(mr);
        std::size_t loadedCount = 0;

        while (!mrReader.eof()) {
            if (mrReader.remaining() < 8) break;
            auto tag = mrReader.readTag();

            if (tag.type == miCOMPRESSED) {
                // Decompress zlib stream
                std::vector<std::uint8_t> uncompressed = ops::zlibDecompress(
                    tag.payload, tag.byteCount);
                MatReader decompReader(uncompressed.data(), uncompressed.size());
                while (!decompReader.eof()) {
                    if (decompReader.remaining() < 8) break;
                    auto [name, v] = decodeMat5Matrix(decompReader, mr);
                    if (!name.empty()) {
                        if (nargout > 0) asStruct.setField(0, name, v);
                        else env.set(name, std::move(v));
                        ++loadedCount;
                    }
                }
            } else if (tag.type == miMATRIX) {
                MatReader subReader(tag.payload - 8, tag.totalAdvance);
                auto [name, v] = decodeMat5Matrix(subReader, mr);
                if (!name.empty()) {
                    if (nargout > 0) asStruct.setField(0, name, v);
                    else env.set(name, std::move(v));
                    ++loadedCount;
                }
            }
        }

        if (loadedCount == 0) {
            throw Error("load: invalid or corrupted MAT5 file");
        }

        if (nargout > 0) outs[0] = std::move(asStruct);
    } else {
        // MAT4 fallback
        loadMat4(resolved.path, *resolved.fs, env, nargout, outs, mr);
    }
}

} // namespace numkit::runtime
