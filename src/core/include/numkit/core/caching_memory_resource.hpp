// core/include/numkit/core/caching_memory_resource.hpp
//
// A memory_resource that recycles freed *large* blocks instead of returning
// them to the OS. Blocks of an identical (size, alignment) are pooled on a
// free list, so a later allocation of that size reuses warm, already-committed
// pages — eliminating the repeated VirtualAlloc/mmap + demand-zero page faults
// that dominate big transient-array churn.
//
// Motivating case: the interpreter evaluating an expression like
// `1./(1+exp(-k*(x-mid)))` over a 10^7-element array materialises several
// 90+ MB temporaries per statement. With a plain malloc-backed resource each
// temporary is a fresh OS reservation whose every page soft-faults on first
// touch (~tens of ms for ~90 MB on Windows); freeing returns it to the OS, so
// the next identical temporary faults all over again. Recycling the block
// keeps the pages resident, turning the per-temporary cost into a memset of
// already-committed memory (which Value::matrix performs anyway).
//
// Small allocations (< minCacheBytes) and any excess beyond maxCachedBytes
// pass straight through to the upstream resource, so the cache never hoards
// tiny blocks and its resident footprint stays bounded.
//
// Thread-safe: the free list is mutex-guarded, but ONLY large
// (>= minCacheBytes) allocate/deallocate calls take the lock — small
// allocations go straight to the (thread-safe) upstream with no locking, so
// the hot small-object path is uncontended.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace numkit {

class CachingMemoryResource : public std::pmr::memory_resource {
public:
    explicit CachingMemoryResource(
        std::pmr::memory_resource *upstream,
        std::size_t minCacheBytes  = 256u * 1024u,            // cache >= 256 KiB
        std::size_t maxCachedBytes = 1024u * 1024u * 1024u)   // cap 1 GiB resident
        noexcept
        : upstream_(upstream ? upstream : std::pmr::new_delete_resource()),
          minCache_(minCacheBytes), maxCached_(maxCachedBytes) {}

    ~CachingMemoryResource() override { releaseCached(); }

    CachingMemoryResource(const CachingMemoryResource &) = delete;
    CachingMemoryResource &operator=(const CachingMemoryResource &) = delete;

    // Return every pooled block to upstream (e.g. to reclaim memory at idle).
    void releaseCached() {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &kv : free_)
            for (void *p : kv.second)
                upstream_->deallocate(p, kv.first.first, kv.first.second);
        free_.clear();
        cachedBytes_ = 0;
    }

private:
    using Key = std::pair<std::size_t, std::size_t>;  // (bytes, alignment)
    struct KeyHash {
        std::size_t operator()(const Key &k) const noexcept {
            return k.first * 1000003u ^ (k.second << 1);
        }
    };

    void *do_allocate(std::size_t bytes, std::size_t align) override {
        if (bytes >= minCache_) {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = free_.find(Key{bytes, align});
            if (it != free_.end() && !it->second.empty()) {
                void *p = it->second.back();
                it->second.pop_back();
                cachedBytes_ -= bytes;
                return p;
            }
        }
        return upstream_->allocate(bytes, align);
    }

    void do_deallocate(void *p, std::size_t bytes, std::size_t align) override {
        if (bytes >= minCache_) {
            std::lock_guard<std::mutex> lk(mu_);
            if (cachedBytes_ + bytes <= maxCached_) {
                free_[Key{bytes, align}].push_back(p);
                cachedBytes_ += bytes;
                return;
            }
        }
        upstream_->deallocate(p, bytes, align);
    }

    bool do_is_equal(const std::pmr::memory_resource &o) const noexcept override {
        return this == &o;
    }

    std::pmr::memory_resource *upstream_;
    std::size_t minCache_;
    std::size_t maxCached_;
    std::mutex  mu_;
    std::unordered_map<Key, std::vector<void *>, KeyHash> free_;
    std::size_t cachedBytes_ = 0;
};

// Process-wide cached resource wrapping new_delete_resource. Used as the
// implicit allocator for default-constructed Engines so the big transient
// arrays the interpreter produces recycle their backing storage. Embedders
// that pass their own memory_resource keep full control and are NOT wrapped.
inline std::pmr::memory_resource *defaultCachedResource() {
    static CachingMemoryResource instance(std::pmr::new_delete_resource());
    return &instance;
}

} // namespace numkit
