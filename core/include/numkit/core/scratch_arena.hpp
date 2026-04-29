// core/include/numkit/core/scratch_arena.hpp
#pragma once

#include <numkit/core/allocator.hpp>

#include <array>
#include <cstddef>
#include <memory_resource>
#include <type_traits>
#include <vector>

namespace numkit {

// Per-call scratch arena for library functions.
//
// Bump-allocates from a stack-mounted inline buffer; overflow spills to
// the user-supplied Allocator via its pmr bridge. All scratches allocated
// through this arena are reclaimed in one move when the arena leaves
// scope — no individual frees.
//
// Intended use: instantiate at the top of every public library function
// that needs temporary buffers, then route every scratch container
// through the arena.
//
//   Value foo(Allocator& alloc, ...) {
//       ScratchArena scratch(alloc);
//       auto a = scratch.vec<double>(n);
//       auto b = scratch.vec<std::size_t>(m);
//       // ... outputs go through `alloc` separately, not through scratch
//   }
//
// Internal helpers should take std::pmr::memory_resource* (obtained via
// scratch.resource()) so they stay decoupled from this concrete type and
// compose with any pmr-aware container.
//
// ── Lifetime contract ─────────────────────────────────────────────────
//
// ScratchArena holds a pointer to the upstream Allocator's pmr bridge;
// the bridge is kept alive by a shared_ptr inside the Allocator. The
// caller MUST ensure the Allocator outlives every ScratchArena built on
// it. In practice this is automatic: both live on the same stack frame,
// the arena is constructed below the Allocator and destroyed before it.
// Don't store a ScratchArena in a heap-allocated object whose lifetime
// can outrun the Allocator passed in.
//
// ── pmr::vector copy / move gotchas ───────────────────────────────────
//
// Copy. pmr containers do NOT propagate the allocator on copy
// construction (uses select_on_container_copy_construction →
// default_resource). `auto X = other;` or `pmr::vector<T> X(other);`
// silently allocate X off-arena. ScratchVec (defined below) closes this
// structurally by deleting its implicit copy ctor — `auto X = other;`
// no longer compiles. Use scratchCopyOf (free function below) for the
// rare case where you genuinely need to duplicate a pmr::vector onto a
// chosen resource. (Slicing-to-base bypasses the deletion; don't.)
//
// Move. Cross-resource moves degrade silently to element-wise moves.
// `pmr::vector<T> dst(mr1); dst = std::move(src_with_mr2);` does NOT
// transfer the buffer — it allocates a new one on mr1 and moves
// elements over (effectively a copy). Same-resource moves are O(1) as
// expected. Practical rule: only move pmr::vector between containers
// you've explicitly constructed with the same resource. When in doubt,
// use scratchCopyOf or stick to the (size, resource) ctor pattern.

// Forward declaration — defined below ScratchArena.
template <class T>
class ScratchVec;

class ScratchArena
{
public:
    explicit ScratchArena(Allocator &upstream)
        : arena_(storage_.data(), storage_.size(), upstream.memoryResource())
    {}

    ScratchArena(const ScratchArena &)            = delete;
    ScratchArena &operator=(const ScratchArena &) = delete;
    ScratchArena(ScratchArena &&)                 = delete;
    ScratchArena &operator=(ScratchArena &&)      = delete;

    std::pmr::memory_resource *resource() noexcept { return &arena_; }

    // Creates a ScratchVec<T> backed by this arena, sized to `n` and
    // value-initialised (zero for arithmetic types). `n == 0` returns an
    // empty vector that callers can grow via reserve()/push_back().
    //
    // ScratchVec<bool> is rejected at compile time — MSVC's pmr::vector<bool>
    // bit-packed proxy specialisation has an init-state miscompilation that
    // bit us on the primes() sieve. Use ScratchVec<std::uint8_t> for boolean
    // masks (footgun #b in this header's doc block).
    template <class T>
    ScratchVec<T> vec(std::size_t n = 0)
    {
        static_assert(!std::is_same_v<T, bool>,
                      "ScratchVec<bool> miscompiles on MSVC (bit-packed proxy "
                      "init bug); use ScratchVec<std::uint8_t> for masks.");
        return ScratchVec<T>(n, &arena_);
    }

private:
    // 64 KiB inline absorbs the typical scratch footprint of every public
    // libs/builtin and libs/signal function — including hash maps for
    // set-ops at small N (where alloc overhead used to dominate) and the
    // multi-array tridiagonal scratches in interp1Spline. Smaller buffers
    // (4 KiB, 16 KiB) measurably regress set-op geomean; larger doesn't
    // help further. Stack burden is fine on every supported target
    // (Windows 1 MB main, emcc 5 MB main / 1 MB worker) since arenas are
    // never nested or recursed. Spillover paths still work for ND-large
    // inputs that exceed even this.
    static constexpr std::size_t kInlineBytes = 65536;

    alignas(std::max_align_t) std::array<std::byte, kInlineBytes> storage_;
    std::pmr::monotonic_buffer_resource arena_;
};

// Scratch container — std::pmr::vector<T> with the implicit copy
// constructor and copy-assignment deleted. Closes footgun #a above
// structurally: `auto X = other_scratchvec;` no longer compiles, where
// it would previously have silently allocated X off-arena via pmr's
// select_on_container_copy_construction. Move stays defaulted (same
// resource → O(1) buffer transfer; cross-resource → element-wise, see
// footgun #d).
//
// All other pmr::vector ctors are inherited unchanged (size, range,
// initializer-list, plus the alloc-aware copy ctor that explicit copies
// like scratchCopyOf use). Slicing to the base type bypasses the
// deletion — don't do it; the docs above are the only guard against
// that mistake.
template <class T>
class ScratchVec : public std::pmr::vector<T>
{
public:
    using base_type = std::pmr::vector<T>;
    using base_type::base_type;
    using base_type::operator=;

    ScratchVec(const ScratchVec &)            = delete;
    ScratchVec &operator=(const ScratchVec &) = delete;

    ScratchVec(ScratchVec &&) noexcept            = default;
    ScratchVec &operator=(ScratchVec &&) noexcept = default;
};

// Returns a copy of `other` backed by `mr`. The only blessed way to
// copy a pmr::vector onto a chosen resource — see ScratchArena's
// "pmr::vector copy / move gotchas" doc above for why. Use this in any
// helper that takes a memory_resource* and needs to copy a pmr::vector.
template <class T>
inline ScratchVec<T>
scratchCopyOf(std::pmr::memory_resource *mr, const std::pmr::vector<T> &other)
{
    return ScratchVec<T>(other, mr);
}

} // namespace numkit
