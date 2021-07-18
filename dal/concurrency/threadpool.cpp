//
// Created by wegam on 2021/7/18.
//

#include <dal/platform/strict.hpp>
#include <dal/concurrency/threadpool.hpp>

namespace Dal {

    using std::operator""s;

    ThreadPool_ ThreadPool_::instance_;
    thread_local size_t ThreadPool_::tlsNum_ = 0;

    void ThreadPool_::ThreadFunc(const size_t& num) {
        tlsNum_ = num;
        Task_  t;
        while(!interrupt_) {
            bool flag = queue_.Pop(t);
            if(flag && !interrupt_)
                t();
        }
    }

    void ThreadPool_::Start(const size_t& nThread) {
        if(!active_) {
            threads_.reserve(nThread);
            for(size_t i = 0; i < nThread; ++i)
                threads_.push_back(std::thread(&ThreadPool_::ThreadFunc, this, i + 1));
            active_ = true;
        }
    }

    void ThreadPool_::Stop() {
        if(active_) {
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

    bool ThreadPool_::ActiveWaite(const TaskHandle_& f) {
        Task_ t;
        bool b = false;

        while(f.wait_for(0s) != std::future_status::ready) {
            if(queue_.TryPop(t)) {
                t();
                b = true;
            }
            else
                f.wait();
        }
        return b;
    }
}