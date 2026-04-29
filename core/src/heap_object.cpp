// src/heap_object.cpp
#include <numkit/core/heap_object.hpp>
#include <numkit/core/value.hpp>

#include <cstring>

namespace numkit {

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
    h->mr = mr;
    // pmr containers require a non-null memory_resource. For matrix /
    // scalar / etc. cellData/structData are null and `mr` may be null
    // (legacy "engine-free" paths); for cell/struct/string the factory
    // already maps null → get_default_resource(), so we mirror that here
    // to keep the contract symmetric.
    auto *cmr = mr ? mr : std::pmr::get_default_resource();
    if (buffer) {
        h->buffer = new DataBuffer(buffer->bytes(), mr);
        std::memcpy(h->buffer->data(), buffer->data(), buffer->bytes());
    }
    if (cellData)
        h->cellData = new std::pmr::vector<Value>(*cellData, cmr);
    if (structData)
        h->structData = new std::pmr::map<std::string, Value>(*structData, cmr);
    if (funcName)
        h->funcName = new std::string(*funcName);
    return h;
}

} // namespace numkit
