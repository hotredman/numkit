// core/src/MThreadPool.cpp — see MThreadPool.hpp for the contract.

#include <numkit/m/core/MThreadPool.hpp>

#include <algorithm>
#include <utility>

namespace numkit::m::detail {

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
        shutdown_ = true;
        ++epoch_;
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

    int k = workers();
    if (max_workers > 0 && max_workers < k)
        k = max_workers;
    if (k <= 1) {
        // Pool unused or capped to 1 — run on the caller's thread.
        fn(0, n);
        return;
    }

    {
        std::unique_lock<std::mutex> lock(mu_);
        task_           = std::move(fn);
        task_n_         = n;
        task_remaining_ = k;
        active_         = k;
        ++epoch_;
    }
    cv_start_.notify_all();

    {
        std::unique_lock<std::mutex> lock(mu_);
        cv_done_.wait(lock, [this] { return task_remaining_ == 0; });
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
            std::unique_lock<std::mutex> lock(mu_);
            cv_start_.wait(lock, [&] { return shutdown_ || epoch_ != seen_epoch; });
            if (shutdown_)
                return;
            seen_epoch = epoch_;
            // Workers beyond the active cap skip this task entirely
            // — they wake on cv_start_ broadcast but immediately go
            // back to sleep, never touching task_remaining_.
            if (id >= active_)
                continue;
            fn         = task_;        // copy under lock
            n          = task_n_;
            k          = active_;
        }

        const std::size_t chunk = (n + static_cast<std::size_t>(k) - 1)
                                  / static_cast<std::size_t>(k);
        const std::size_t start = static_cast<std::size_t>(id) * chunk;
        const std::size_t end   = std::min(start + chunk, n);
        if (start < end)
            fn(start, end);

        {
            std::lock_guard<std::mutex> lock(mu_);
            if (--task_remaining_ == 0)
                cv_done_.notify_one();
        }
    }
}

} // namespace numkit::m::detail
