// include/MHeapObject.hpp
#pragma once

#include "MDataBuffer.hpp"
#include "MDims.hpp"
#include "MMType.hpp"

#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace numkit::m::m {

class MValue; // forward decl

// ============================================================
// HeapObject — ref-counted storage for non-scalar values
//
// Holds arrays, cells, structs, func handles.
// Shared via COW (copy-on-write) through refCount.
// ============================================================
struct HeapObject
{
    std::atomic<int> refCount{1};
    MType type = MType::EMPTY;
    Dims dims;
    Allocator *allocator = nullptr;

    // Array data (DOUBLE, INT*, UINT*, LOGICAL, CHAR, COMPLEX)
    DataBuffer *buffer = nullptr;

    // Extended data — allocated only for CELL, STRUCT, FUNC_HANDLE
    std::vector<MValue> *cellData = nullptr;
    std::map<std::string, MValue> *structData = nullptr;
    std::string *funcName = nullptr;

    // Capacity for appendScalar amortization
    size_t appendCapacity = 0;

    HeapObject() = default;
    ~HeapObject();

    HeapObject(const HeapObject &) = delete;
    HeapObject &operator=(const HeapObject &) = delete;

    void addRef() { refCount.fetch_add(1, std::memory_order_relaxed); }
    bool release() { return refCount.fetch_sub(1, std::memory_order_acq_rel) == 1; }

    // Deep clone for COW
    HeapObject *clone() const;
};

} // namespace numkit::m::m
