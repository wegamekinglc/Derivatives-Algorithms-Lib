//
// Created by wegam on 2021/7/18.
//

#include <functional>
#include <algorithm>
#include <dal/platform/strict.hpp>
#include <dal/concurrency/threadpool.hpp>

namespace Dal {

    ThreadPool_ ThreadPool_::instance_;
    thread_local size_t ThreadPool_::tlsNum_ = 0;

    void ThreadPool_::ThreadFunc(const size_t& num) {
        tlsNum_ = num;
        Task_ t;
        while (!interrupt_) {
            if (queue_.Pop(t))
                t();
        }
    }

    void ThreadPool_::Start(size_t n_threads, bool restart) {
        if (n_threads <= 0 || n_threads > std::thread::hardware_concurrency())
            n_threads = std::thread::hardware_concurrency();

        if (active_ && restart)
            Stop();

        if (!active_) {
            threads_.reserve(n_threads - 1);
            for (size_t i = 0; i < n_threads - 1; ++i)
                threads_.emplace_back(&ThreadPool_::ThreadFunc, this, i + 1);
            active_ = true;
        }
    }

    void ThreadPool_::Stop() {
        if (active_) {
            interrupt_ = true;
            queue_.Interrupt();
            for_each(threads_.begin(), threads_.end(), std::mem_fn(&std::thread::join));
            threads_.clear();
            queue_.Clear();
            queue_.ResetInterrupt();
            active_ = false;
            interrupt_ = false;
        }
    }

    bool ThreadPool_::ActiveWait(const TaskHandle_& f) {
        Task_ t;
        bool b = false;
        while (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            if (queue_.TryPop(t)) {
                t();
                b = true;
            } else
                f.wait();
        }
        return b;
    }
} // namespace Dal