#include <atomic>
#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

class DiagnosticException_ : public std::runtime_error {
public:
    explicit DiagnosticException_(const std::string& message) : std::runtime_error(message) {}
};

class DiagnosticError_ : public DiagnosticException_ {
public:
    explicit DiagnosticError_(const std::string& message) : DiagnosticException_(message) {}
};

void ConsumeAndRethrow(std::future<bool>* future) {
    std::exception_ptr failure;
    try {
        static_cast<void>(future->get());
    } catch (...) {
        failure = std::current_exception();
    }
    if (failure)
        std::rethrow_exception(failure);
}

int main(int argc, char**) {
    const bool explicitSynchronization = argc > 1;
    std::atomic<bool> readFinished{false};
    std::packaged_task<bool()> task([]() -> bool { throw DiagnosticError_(std::string(256, 'x')); });
    auto future = task.get_future();
    std::thread worker([work = std::move(task), &readFinished, explicitSynchronization]() mutable {
        work();
        // Delay task destruction; the relaxed mode adds no happens-before edge.
        while (!readFinished.load(explicitSynchronization ? std::memory_order_acquire : std::memory_order_relaxed))
            std::this_thread::yield();
    });
    size_t messageSize = 0;
    try {
        ConsumeAndRethrow(&future);
    } catch (const DiagnosticException_& error) {
        messageSize = std::string(error.what()).size();
    }
    readFinished.store(true, explicitSynchronization ? std::memory_order_release : std::memory_order_relaxed);
    worker.join();
    std::cout << "messageSize=" << messageSize << ", explicitSynchronization=" << explicitSynchronization << '\n';
    return messageSize == 256 ? 0 : 1;
}
