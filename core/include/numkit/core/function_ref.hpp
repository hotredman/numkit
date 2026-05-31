// core/include/numkit/core/function_ref.hpp
//
// numkit::function_ref<Sig> — lightweight, non-owning callable view.
//
// Two-pointer type-erased reference to any callable matching the
// signature `Sig`. Cheap to copy (16 bytes on 64-bit), no allocation,
// no virtual dispatch overhead (single indirect call through a thunk).
// The lifetime of the underlying callable is the caller's
// responsibility — function_ref does NOT own it.
//
// Used for "matlab-style function handle" callbacks in the numerical
// library API (fzero, integral, fminsearch, cellfun, …). The engine
// adapter builds a stack-resident lambda that wraps a function-handle
// Value, then passes a function_ref view to the library function.
// The library function calls the callback as many times as needed and
// returns; the lambda + function_ref both live in the adapter frame.
//
// Inspired by C++26's std::function_ref and tcb::function_ref.

#pragma once

#include <type_traits>
#include <utility>

namespace numkit {

template <class Sig>
class function_ref;  // primary template undefined — only spec'd below

template <class R, class... Args>
class function_ref<R(Args...)>
{
public:
    /// @brief Default-construct an empty function_ref (calling it is UB).
    /// Convenience for "optional callback" use cases. Use the operator
    /// to test for empty — if `!fn` returns true, the ref is empty.
    constexpr function_ref() noexcept = default;

    /// @brief Wrap any callable `F` compatible with `R(Args...)`.
    ///
    /// `F` is captured by reference — its lifetime must outlive the
    /// last use of this function_ref.
    ///
    /// Disabled when `F` is `function_ref` itself (so copy/move ctors
    /// take precedence) and when `F` is not actually callable with
    /// the right signature (better diagnostics).
    template <class F,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<F>, function_ref> &&
                  std::is_invocable_r_v<R, F &, Args...>>>
    function_ref(F &&f) noexcept
        : obj_(const_cast<void *>(static_cast<const void *>(&f))),
          thunk_(&thunkFor<std::remove_reference_t<F>>)
    {}

    /// @brief Invoke the wrapped callable. UB if empty.
    R operator()(Args... args) const
    {
        return thunk_(obj_, std::forward<Args>(args)...);
    }

    /// @brief `true` iff a callable is wrapped.
    explicit operator bool() const noexcept { return thunk_ != nullptr; }

private:
    using thunk_t = R (*)(void *, Args...);

    template <class F>
    static R thunkFor(void *obj, Args... args)
    {
        return (*static_cast<F *>(obj))(std::forward<Args>(args)...);
    }

    void *  obj_   = nullptr;
    thunk_t thunk_ = nullptr;
};

} // namespace numkit
