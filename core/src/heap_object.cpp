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
    h->allocator = allocator;
    if (buffer) {
        h->buffer = new DataBuffer(buffer->bytes(), allocator);
        std::memcpy(h->buffer->data(), buffer->data(), buffer->bytes());
    }
    if (cellData)
        h->cellData = new std::vector<Value>(*cellData);
    if (structData)
        h->structData = new std::map<std::string, Value>(*structData);
    if (funcName)
        h->funcName = new std::string(*funcName);
    return h;
}

} // namespace numkit
