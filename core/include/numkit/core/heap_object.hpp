// include/heap_object.hpp
#pragma once

#include <numkit/core/data_buffer.hpp>
#include <numkit/core/dims.hpp>
#include <numkit/core/value_type.hpp>

#include <atomic>
#include <map>
#include <memory_resource>
#include <string>
#include <vector>

namespace numkit {

class Value; // forward decl

// ============================================================
// HeapObject — ref-counted storage for non-scalar values
//
// Holds arrays, cells, structs, func handles.
// Shared via COW (copy-on-write) through refCount.
// ============================================================
struct HeapObject
{
    std::atomic<int> refCount{1};
    ValueType type = ValueType::EMPTY;
    Dims dims;
    std::pmr::memory_resource *mr = nullptr;  // not owned; outlives the heap

    // Array data (DOUBLE, INT*, UINT*, LOGICAL, CHAR, COMPLEX)
    DataBuffer *buffer = nullptr;

    // Extended data — allocated only for CELL, STRUCT, FUNC_HANDLE
    std::vector<Value> *cellData = nullptr;
    std::map<std::string, Value> *structData = nullptr;
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

} // namespace numkit
