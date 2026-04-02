// include/MLabVMValue.hpp
//
// Lightweight tagged value for VM register file.
// Optimized for the common case: scalar double.
//
// Layout: 16 bytes total
//   - tag (uint8_t): EMPTY, SCALAR, MVALUE
//   - scalar (double): inline value for SCALAR tag
//   - mvalue (MValue*): heap pointer for general case
//
// Design principles:
//   1. Scalar operations never allocate — pure double arithmetic
//   2. Conversion to/from MValue only when needed (function calls, array ops)
//   3. Move semantics for MValue ownership
//   4. No virtual dispatch, no allocator for scalar path

#pragma once

#include "MLabValue.hpp"
#include <cassert>
#include <cmath>
#include <utility>

namespace mlab {

class VMValue
{
public:
    enum class Tag : uint8_t {
        EMPTY,      // no value
        SCALAR,     // inline double
        MVALUE,     // owned MValue on heap
    };

    // Default: empty
    VMValue() : tag_(Tag::EMPTY), scalar_(0.0), mvalue_(nullptr) {}

    // Construct from scalar — zero allocation
    static VMValue fromScalar(double v)
    {
        VMValue r;
        r.tag_ = Tag::SCALAR;
        r.scalar_ = v;
        return r;
    }

    // Construct from MValue — takes ownership via move
    static VMValue fromMValue(MValue &&v)
    {
        VMValue r;
        if (v.isScalar() && v.type() == MType::DOUBLE) {
            // Demote to inline scalar — avoid heap
            r.tag_ = Tag::SCALAR;
            r.scalar_ = v.toScalar();
        } else if (v.isEmpty()) {
            r.tag_ = Tag::EMPTY;
        } else {
            r.tag_ = Tag::MVALUE;
            r.mvalue_ = new MValue(std::move(v));
        }
        return r;
    }

    // Construct from MValue ref — copies if needed
    static VMValue fromMValue(const MValue &v)
    {
        VMValue r;
        if (v.isScalar() && v.type() == MType::DOUBLE) {
            r.tag_ = Tag::SCALAR;
            r.scalar_ = v.toScalar();
        } else if (v.isEmpty()) {
            r.tag_ = Tag::EMPTY;
        } else {
            r.tag_ = Tag::MVALUE;
            r.mvalue_ = new MValue(v);
        }
        return r;
    }

    ~VMValue() { clear(); }

    // Move
    VMValue(VMValue &&o) noexcept
        : tag_(o.tag_), scalar_(o.scalar_), mvalue_(o.mvalue_)
    {
        o.tag_ = Tag::EMPTY;
        o.mvalue_ = nullptr;
    }

    VMValue &operator=(VMValue &&o) noexcept
    {
        if (this != &o) {
            clear();
            tag_ = o.tag_;
            scalar_ = o.scalar_;
            mvalue_ = o.mvalue_;
            o.tag_ = Tag::EMPTY;
            o.mvalue_ = nullptr;
        }
        return *this;
    }

    // Copy
    VMValue(const VMValue &o) : tag_(o.tag_), scalar_(o.scalar_), mvalue_(nullptr)
    {
        if (o.tag_ == Tag::MVALUE && o.mvalue_)
            mvalue_ = new MValue(*o.mvalue_);
    }

    VMValue &operator=(const VMValue &o)
    {
        if (this != &o) {
            clear();
            tag_ = o.tag_;
            scalar_ = o.scalar_;
            if (o.tag_ == Tag::MVALUE && o.mvalue_)
                mvalue_ = new MValue(*o.mvalue_);
            else
                mvalue_ = nullptr;
        }
        return *this;
    }

    // ── Queries ──────────────────────────────────────────────

    Tag tag() const { return tag_; }
    bool isEmpty() const { return tag_ == Tag::EMPTY; }
    bool isScalar() const { return tag_ == Tag::SCALAR; }
    bool isMValue() const { return tag_ == Tag::MVALUE; }

    // ── Scalar access (no allocation) ────────────────────────

    double scalar() const
    {
        assert(tag_ == Tag::SCALAR);
        return scalar_;
    }

    void setScalar(double v)
    {
        clear();
        tag_ = Tag::SCALAR;
        scalar_ = v;
    }

    // ── MValue access ────────────────────────────────────────

    const MValue &mvalue() const
    {
        assert(tag_ == Tag::MVALUE && mvalue_);
        return *mvalue_;
    }

    MValue &mvalue()
    {
        assert(tag_ == Tag::MVALUE && mvalue_);
        return *mvalue_;
    }

    // Take ownership of internal MValue (moves it out)
    MValue takeMValue()
    {
        if (tag_ == Tag::MVALUE && mvalue_) {
            MValue result = std::move(*mvalue_);
            delete mvalue_;
            mvalue_ = nullptr;
            tag_ = Tag::EMPTY;
            return result;
        }
        if (tag_ == Tag::SCALAR)
            return MValue::scalar(scalar_);
        return MValue::empty();
    }

    // Set to MValue (takes ownership)
    void setMValue(MValue &&v)
    {
        // Try to demote to scalar
        if (v.isScalar() && v.type() == MType::DOUBLE) {
            clear();
            tag_ = Tag::SCALAR;
            scalar_ = v.toScalar();
            return;
        }
        if (v.isEmpty()) {
            clear();
            tag_ = Tag::EMPTY;
            return;
        }
        if (tag_ == Tag::MVALUE && mvalue_) {
            *mvalue_ = std::move(v);
        } else {
            clear();
            tag_ = Tag::MVALUE;
            mvalue_ = new MValue(std::move(v));
        }
    }

    // ── Conversion to MValue (for function calls, display) ──

    MValue toMValue(Allocator *alloc = nullptr) const
    {
        switch (tag_) {
        case Tag::SCALAR: return MValue::scalar(scalar_, alloc);
        case Tag::MVALUE: return *mvalue_;
        default: return MValue::empty();
        }
    }

    // ── Convenience for common checks ────────────────────────

    double toDouble() const
    {
        if (tag_ == Tag::SCALAR) return scalar_;
        if (tag_ == Tag::MVALUE) return mvalue_->toScalar();
        return 0.0;
    }

    bool toBool() const
    {
        if (tag_ == Tag::SCALAR) return scalar_ != 0.0;
        if (tag_ == Tag::MVALUE) return mvalue_->toBool();
        return false;
    }

private:
    void clear()
    {
        if (tag_ == Tag::MVALUE) {
            delete mvalue_;
            mvalue_ = nullptr;
        }
        tag_ = Tag::EMPTY;
        scalar_ = 0.0;
    }

    Tag tag_;
    double scalar_;
    MValue *mvalue_;
};

} // namespace mlab