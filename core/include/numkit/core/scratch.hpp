// core/include/numkit/core/scratch.hpp
//
// numkit memory layer — two thin classes over std::pmr.
//
// `std::pmr::memory_resource*` is the single allocation primitive across
// the codebase. Public APIs accept it directly. Embedders supply their
// own memory_resource (subclass it, or use one of the std::pmr-provided
// resources like get_default_resource(), pool resources, etc.).
//
// ── ScratchArena ──────────────────────────────────────────────────────
//
// Per-call bump arena. IS-A std::pmr::monotonic_buffer_resource —
// pass `&arena` directly to anything pmr-aware (pmr containers, the
// nested ScratchVec, etc.). 64 KiB inline storage absorbs the typical
// scratch footprint of every public libs/builtin and libs/signal
// function (incl. set-op hash maps at small N and tridiagonal scratches
// in interp1Spline). Spillover paths still work for inputs that exceed
// this. Stack burden +60 KiB per arena is fine on every supported
// target (Windows 1 MB main, emcc 5 MB main / 1 MB worker) since
// arenas are scope-confined to a single public function call (no
// nesting / no recursion).
//
// Lifetime: the upstream memory_resource MUST outlive every ScratchArena
// built on it, AND every Value/DataBuffer that flowed through that
// memory_resource. In practice the upstream comes from Engine and
// outlives all Values it produced — standard pmr lifetime contract.
//
// ── ScratchVec<T> ─────────────────────────────────────────────────────
//
// std::pmr::vector<T> with two structural footgun closures:
//
//   #a  Implicit copy ctor + copy-assignment DELETED.
//       `auto X = other_scratchvec;` is a compile error. Without the
//       deletion it would have silently allocated X off-arena via pmr's
//       select_on_container_copy_construction → default_resource. Use
//       the alloc-aware copy ctor (inherited from base) explicitly when
//       a duplicate is genuinely intended:
//           ScratchVec<T> dup(other_scratchvec, mr);
//
//   #b  bool rejected at compile time via static_assert.
//       MSVC's pmr::vector<bool> bit-packed proxy has an init-state
//       miscompilation that bit us on the primes() sieve. Use
//       ScratchVec<std::uint8_t> for boolean masks.
//
// Move stays defaulted (same resource → O(1) buffer transfer; cross-
// resource degrades to element-wise — same as plain pmr::vector).
//
// All other base ctors are inherited via `using base_type::base_type`.
// The standard pmr ctor `ScratchVec<T>(n, memory_resource*)` is the
// canonical way to construct.
//
// Slicing-to-base (`pmr::vector<T> X(scratch_vec);`) bypasses the
// deletions. Don't do it.

#pragma once

#include <array>
#include <cstddef>
#include <memory_resource>
#include <type_traits>
#include <vector>

namespace numkit {

template <class T>
class ScratchVec : public std::pmr::vector<T>
{
    static_assert(!std::is_same_v<T, bool>,
                  "ScratchVec<bool> miscompiles on MSVC (bit-packed proxy "
                  "init bug); use ScratchVec<std::uint8_t> for masks.");
public:
    using base_type = std::pmr::vector<T>;
    using base_type::base_type;
    using base_type::operator=;

    ScratchVec(const ScratchVec &)            = delete;
    ScratchVec &operator=(const ScratchVec &) = delete;

    ScratchVec(ScratchVec &&) noexcept            = default;
    ScratchVec &operator=(ScratchVec &&) noexcept = default;
};

class ScratchArena : public std::pmr::monotonic_buffer_resource
{
public:
    explicit ScratchArena(std::pmr::memory_resource *upstream)
        : std::pmr::monotonic_buffer_resource(
              storage_.data(), storage_.size(),
              upstream ? upstream : std::pmr::get_default_resource()) {}

    ScratchArena(const ScratchArena &)            = delete;
    ScratchArena &operator=(const ScratchArena &) = delete;
    ScratchArena(ScratchArena &&)                 = delete;
    ScratchArena &operator=(ScratchArena &&)      = delete;

private:
    static constexpr std::size_t kInlineBytes = 65536;
    alignas(std::max_align_t) std::array<std::byte, kInlineBytes> storage_;
};

}  // namespace numkit
