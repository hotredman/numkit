// include/numkit/graphics/graphics_handle.hpp
//
// Graphics-handle object payload — the Value-side half of the handle
// registry (bugs/closed/graphics/plot-family-no-return-value.md).
//
// Layering: the registry (id → HandleRecord) lives in FigureManager
// (figure layer, no Engine). This header carries the NativePayload that
// a handle Value holds: just the registry id. It depends only on the
// value layer, so three consumers can share it without layering knots:
//   • the graphics plot bodies (core-free, e.g. close(figureHandle)),
//   • the bundle install hub (graphics_library.cpp — accessor layer),
//   • the builtin layer (general_reg.cpp — unified delete dispatch).
#pragma once

#include <numkit/value/object.hpp>
#include <numkit/value/value.hpp>

#include <memory>
#include <string>

namespace numkit {

// NativePayload carrying the FigureManager handle-registry id.
struct GraphicsHandlePayload : public NativePayload
{
    int handleId = 0;
    std::shared_ptr<NativePayload> clone() const override
    {
        auto p = std::make_shared<GraphicsHandlePayload>();
        p->handleId = handleId;
        return p;
    }
};

// Registry id of a graphics-handle Value, or -1 when `v` is not one.
// Payload-based (not class-name-based): class(h) reports the MATLAB
// class (matlab.graphics.chart.primitive.Line, …), so the class name is
// NOT a discriminator — the payload is.
inline int graphicsHandleIdOf(const Value &v)
{
    if (!v.isObject())
        return -1;
    auto *st = v.objectStateConst();
    if (!st || !st->native)
        return -1;
    auto *p = dynamic_cast<const GraphicsHandlePayload *>(st->native.get());
    return p ? p->handleId : -1;
}

// Handle object Value wrapping registry id `id` with MATLAB class name
// `className` (that string is what class()/isa() report).
inline Value makeGraphicsHandleValue(const std::string &className, int id,
                                     std::pmr::memory_resource *mr)
{
    auto st = std::make_shared<ObjectState>(mr);
    auto payload = std::make_shared<GraphicsHandlePayload>();
    payload->handleId = id;
    st->native = payload;
    return Value::object(className, std::move(st), /*isHandle=*/true, mr);
}

} // namespace numkit
