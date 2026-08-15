#include <numkit/ops/thread_pool.hpp>

#include <algorithm>
#include <utility>
#include <hwy/cache_control.h>

namespace numkit::detail {

inline void spin_pause() noexcept {
    hwy::Pause();
}

// True while the current thread is executing a pool task body. A nested
// run() (a parallel body that itself calls parallel_for) is then run inline
// rather than re-dispatched, which would deadlock.
namespace {
thread_local bool t_inPool = false;
}

ThreadPool &ThreadPool::global()
{
    // hardware_concurrency() can return 0 when the runtime has no idea
    // (rare on desktop, occasionally on WSL/containers); fall back to 1
    // so the pool is well-formed (and `run()` then takes the sequential
    // shortcut anyway).
    static const int n = std::max(1u, std::thread::hardware_concurrency());
    static ThreadPool instance(n);
    return instance;
}

ThreadPool::ThreadPool(int n_workers)
{
    if (n_workers <= 1)
        return; // workers_ stays empty; run() degrades to fn(0, n)
    workers_.reserve(static_cast<std::size_t>(n_workers));
    for (int i = 0; i < n_workers; ++i)
        workers_.emplace_back([this, i] { worker_loop(i); });
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mu_);
        shutdown_.store(true, std::memory_order_release);
        epoch_.fetch_add(1, std::memory_order_release);
    }
    cv_start_.notify_all();
    for (auto &t : workers_)
        if (t.joinable())
            t.join();
}

void ThreadPool::run(std::size_t n, std::function<void(std::size_t, std::size_t)> fn,
                     int max_workers)
{
    if (n == 0)
        return;

    // Nested dispatch (a worker's task body itself called parallel_for): run
    // inline. Re-dispatching to the same pool would deadlock — this thread
    // would block on sibling chunks that cannot make progress.
    if (t_inPool) {
        fn(0, n);
        return;
    }

    int k = workers();
    if (max_workers > 0 && max_workers < k)
        k = max_workers;
    if (k <= 1) {
        // Pool unused or capped to 1 — run on the caller's thread.
        fn(0, n);
        return;
    }

    // Serialize concurrent submitters (the pool's task_/epoch_ slots are
    // single-task). Held across publish+wait; workers use mu_, not this, and a
    // nested run() already returned above, so this is never contended by a
    // worker.
    std::lock_guard<std::mutex> submitLock(submit_mu_);

    {
        std::unique_lock<std::mutex> lock(mu_);
        task_           = std::move(fn);
        task_n_         = n;
        task_remaining_.store(k, std::memory_order_release);
        active_         = k;
        epoch_.fetch_add(1, std::memory_order_release);
    }
    cv_start_.notify_all();

    {
        for (int spin = 0; spin < 2000; ++spin) {
            if (task_remaining_.load(std::memory_order_acquire) == 0) break;
            spin_pause();
        }
        if (task_remaining_.load(std::memory_order_acquire) != 0) {
            std::unique_lock<std::mutex> lock(mu_);
            cv_done_.wait(lock, [this] { return task_remaining_.load(std::memory_order_acquire) == 0; });
        }
        // Drop the task closure under the lock so its destructors run
        // in a predictable place (avoids capturing locals living on
        // the caller's frame for any longer than necessary).
        task_ = nullptr;
    }
}

void ThreadPool::worker_loop(int id)
{
    int seen_epoch = 0;
    while (true) {
        std::function<void(std::size_t, std::size_t)> fn;
        std::size_t n;
        int         k;

        {
            for (int spin = 0; spin < 2000; ++spin) {
                if (shutdown_.load(std::memory_order_acquire) || epoch_.load(std::memory_order_acquire) != seen_epoch) break;
                spin_pause();
            }
            if (!(shutdown_.load(std::memory_order_acquire) || epoch_.load(std::memory_order_acquire) != seen_epoch)) {
                std::unique_lock<std::mutex> lock(mu_);
                cv_start_.wait(lock, [&] { return shutdown_.load(std::memory_order_acquire) || epoch_.load(std::memory_order_acquire) != seen_epoch; });
            }
            if (shutdown_.load(std::memory_order_acquire))
                return;
            seen_epoch = epoch_.load(std::memory_order_acquire);
            // Workers beyond the active cap skip this task entirely
            // — they wake on cv_start_ broadcast but immediately go
            // back to sleep, never touching task_remaining_.
            if (id >= active_)
                continue;
            fn         = task_;        // safe to read without lock because epoch was bumped after task_ was written
            n          = task_n_;
            k          = active_;
        }

        const std::size_t chunk = (n + static_cast<std::size_t>(k) - 1)
                                  / static_cast<std::size_t>(k);
        const std::size_t start = static_cast<std::size_t>(id) * chunk;
        const std::size_t end   = std::min(start + chunk, n);
        if (start < end) {
            t_inPool = true;     // mark so a nested run() on this thread inlines
            fn(start, end);
            t_inPool = false;
        }

        {
            if (task_remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lock(mu_);
                cv_done_.notify_one();
            }
        }
    }
}

} // namespace numkit::detail
