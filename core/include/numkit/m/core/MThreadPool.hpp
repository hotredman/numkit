// core/include/numkit/m/core/MThreadPool.hpp
//
// Persistent worker pool used by SIMD kernels above their parallel
// thresholds. The pool is built once at first use (singleton) with
// `std::thread::hardware_concurrency()` workers and stays alive for
// the lifetime of the process. Each `run()` call hands a function
// `fn(start, end)` to the workers, splits [0, n) evenly, and blocks
// until every chunk has finished.
//
// This header is only present when NUMKIT_WITH_THREADS is defined at
// build time. The companion `MParallelFor.hpp` includes it
// conditionally and falls back to a sequential `fn(0, n)` otherwise,
// so call sites stay portable across the threaded and non-threaded
// builds.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace numkit::m::detail {

class ThreadPool
{
public:
    // Process-wide singleton. Sized to hardware_concurrency() at first call.
    static ThreadPool &global();

    explicit ThreadPool(int n_workers);
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    int workers() const { return static_cast<int>(workers_.size()); }

    // Run `fn(start, end)` over an even split of [0, n) across workers,
    // blocking until every chunk finishes. With one or zero workers this
    // simply calls `fn(0, n)` on the calling thread.
    void run(std::size_t n, std::function<void(std::size_t, std::size_t)> fn);

private:
    void worker_loop(int id);

    std::vector<std::thread>         workers_;
    std::mutex                       mu_;
    std::condition_variable          cv_start_;
    std::condition_variable          cv_done_;

    // Current task. `epoch_` is bumped on every dispatch so workers
    // can tell a new task apart from a spurious wake-up.
    std::function<void(std::size_t, std::size_t)> task_;
    std::size_t                      task_n_         = 0;
    int                              task_remaining_ = 0;
    int                              epoch_          = 0;
    bool                             shutdown_       = false;
};

} // namespace numkit::m::detail
