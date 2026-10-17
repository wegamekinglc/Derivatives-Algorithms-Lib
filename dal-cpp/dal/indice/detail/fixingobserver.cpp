#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::Detail {
    FixingReadObserver_*& FixingReadObserver() {
        static thread_local FixingReadObserver_* observer = nullptr;
        return observer;
    }
} // namespace Dal::Detail
