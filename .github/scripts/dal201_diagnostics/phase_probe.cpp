//
// Created by dal-implementer on 2026/9/14.
//

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/math/optimization/underdetermined.hpp>

namespace {
    using Clock_ = std::chrono::steady_clock;
    struct Entry_ {
        const char* phase_;
        int tag_, invocation_, gradient_, rows_ = 0, cols_ = 0, nodes_ = 0;
        uint64_t nanos_, inputs_ = 0, residuals_ = 0, jacobian_ = 0;
    };
    Entry_ entries[2048];
    size_t count = 0;
    bool overflow = false;
    thread_local int tag = 0;
    thread_local int invocation = 0;
    thread_local int gradient = 0;
    thread_local Clock_::time_point invocationStart, recordingStart;
    thread_local bool recording = false;
    using Gradient_ = std::unique_ptr<Dal::Underdetermined::Jacobian_> (*)(const Dal::Underdetermined::Function_*,
                                                                           const Dal::Vector_<>&,
                                                                           const Dal::Vector_<>&);
    Gradient_ actualGradient = nullptr;
    using Residual_ = Dal::Vector_<> (*)(const Dal::Underdetermined::Function_*, const Dal::Vector_<>&);
    Residual_ actualResidual = nullptr;

    uint64_t Elapsed(Clock_::time_point start) { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - start).count(); }

    void Add(const Entry_& entry) {
        if (count == std::size(entries)) {
            overflow = true;
            return;
        }
        entries[count++] = entry;
    }

    Entry_ Event(const char* phase, uint64_t nanos) { return {phase, tag, invocation, gradient, 0, 0, 0, nanos}; }

    template <class F_> F_ Symbol(const char* name) {
        auto* library = dlopen(std::getenv("DAL201_BINARY"), RTLD_LAZY | RTLD_NOLOAD);
        auto function = reinterpret_cast<F_>(library ? dlsym(library, name) : nullptr);
        if (!function)
            std::abort();
        return function;
    }

    uint64_t Hash(uint64_t hash, double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return (hash ^ bits) * 1099511628211ULL;
    }

    std::unique_ptr<Dal::Underdetermined::Jacobian_>
    Gradient(const Dal::Underdetermined::Function_* function, const Dal::Vector_<>& parameters, const Dal::Vector_<>& residuals) {
        if (!tag)
            return actualGradient(function, parameters, residuals);
        ++gradient;
        auto start = Clock_::now();
        auto result = actualGradient(function, parameters, residuals);
        Add(Event("gradient_inclusive", Elapsed(start)));
        return result;
    }

    Dal::Vector_<> Residual(const Dal::Underdetermined::Function_* function, const Dal::Vector_<>& parameters) {
        if (!tag)
            return actualResidual(function, parameters);
        auto start = Clock_::now();
        auto result = actualResidual(function, parameters);
        auto event = Event("double_residual", Elapsed(start));
        for (double value : result)
            event.residuals_ = Hash(event.residuals_, value);
        Add(event);
        return result;
    }
} // namespace

extern "C" int Dal201Install(void* anchor, size_t anchorOffset, size_t gradientSlot, size_t gradientOffset, size_t residualOffset) {
    Dl_info info{};
    if (!dladdr(anchor, &info) || reinterpret_cast<size_t>(info.dli_fbase) + anchorOffset != reinterpret_cast<size_t>(anchor))
        return 1;
    auto base = reinterpret_cast<size_t>(info.dli_fbase);
    auto** slot = reinterpret_cast<void**>(base + gradientSlot);
    if (reinterpret_cast<size_t>(*slot) != base + gradientOffset || actualGradient || reinterpret_cast<size_t>(*(slot - 1)) != base + residualOffset)
        return 2;
    actualGradient = reinterpret_cast<Gradient_>(*slot);
    actualResidual = reinterpret_cast<Residual_>(*(slot - 1));
    const auto pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto page = reinterpret_cast<size_t>(slot - 1) & ~(pageSize - 1);
    auto end = (reinterpret_cast<size_t>(slot + 1) + pageSize - 1) & ~(pageSize - 1);
    if (mprotect(reinterpret_cast<void*>(page), end - page, PROT_READ | PROT_WRITE))
        return 3;
    *(slot - 1) = reinterpret_cast<void*>(&Residual);
    *slot = reinterpret_cast<void*>(&Gradient);
    return mprotect(reinterpret_cast<void*>(page), end - page, PROT_READ) ? 4 : 0;
}

extern "C" void Dal201Tag(int value) {
    if (tag && !value)
        Add(Event("target_inclusive", Elapsed(invocationStart)));
    tag = value;
    if (tag) {
        ++invocation;
        gradient = 0;
        recording = false;
        invocationStart = Clock_::now();
    }
}

extern "C" int Dal201Dump(const char* filename) {
    auto* output = std::fopen(filename, "w");
    if (!output)
        return 1;
    for (size_t i = 0; i < count; ++i) {
        const auto& e = entries[i];
        std::fprintf(output,
                     "{\"phase\":\"%s\",\"tag\":%d,\"invocation\":%d,\"gradient\":%d,\"ns\":%llu,"
                     "\"rows\":%d,\"cols\":%d,\"nodes\":%d,\"inputs\":\"%llu\",\"residuals\":\"%llu\",\"jacobian\":\"%llu\"}\n",
                     e.phase_, e.tag_, e.invocation_, e.gradient_, static_cast<unsigned long long>(e.nanos_), e.rows_, e.cols_, e.nodes_,
                     static_cast<unsigned long long>(e.inputs_), static_cast<unsigned long long>(e.residuals_),
                     static_cast<unsigned long long>(e.jacobian_));
    }
    int result = std::fclose(output);
    return result || overflow;
}

namespace Dal {
    Vector_<AAD::Number_> RegisterCurveParameters(const Vector_<>& parameters) {
        using Fn_ = Vector_<AAD::Number_> (*)(const Vector_<>&);
        static auto actual = Symbol<Fn_>("_ZN3Dal23RegisterCurveParametersERKNS_7Vector_IdEE");
        if (!tag)
            return actual(parameters);
        auto start = Clock_::now();
        auto result = actual(parameters);
        Add(Event("register", Elapsed(start)));
        return result;
    }

    Matrix_<> HarvestCurveJacobian(AAD::Tape_& tape, Vector_<AAD::Number_>& inputs, Vector_<AAD::Number_>& residuals, const Vector_<int>& widths) {
        using Fn_ = Matrix_<> (*)(AAD::Tape_&, Vector_<AAD::Number_>&, Vector_<AAD::Number_>&, const Vector_<int>&);
        static auto actual = Symbol<Fn_>("_ZN3Dal20HarvestCurveJacobianERNS_3AAD5Tape_ERNS_7Vector_INS0_7Number_EEES6_RKNS3_IiEE");
        if (!tag)
            return actual(tape, inputs, residuals, widths);
        if (!recording)
            std::abort();
        Add(Event("recording_to_harvest", Elapsed(recordingStart)));
        recording = false;
        auto event = Event("harvest", 0);
        event.rows_ = residuals.size();
        event.cols_ = inputs.size();
        event.nodes_ = std::distance(tape.nodes_.Begin(), tape.nodes_.End());
        for (const auto& input : inputs)
            event.inputs_ = Hash(event.inputs_, Value(input));
        for (const auto& value : residuals)
            event.residuals_ = Hash(event.residuals_, Value(value));
        auto start = Clock_::now();
        auto result = actual(tape, inputs, residuals, widths);
        event.nanos_ = Elapsed(start);
        for (int row = 0; row < result.Rows(); ++row)
            for (int col = 0; col < result.Cols(); ++col)
                event.jacobian_ = Hash(event.jacobian_, result(row, col));
        Add(event);
        return result;
    }
} // namespace Dal

namespace Dal::AAD {
    void NewRecording(Tape_& tape) {
        using Fn_ = void (*)(Tape_&);
        static auto actual = Symbol<Fn_>("_ZN3Dal3AAD12NewRecordingERNS0_5Tape_E");
        actual(tape);
        if (tag) {
            recordingStart = Clock_::now();
            recording = true;
        }
    }
} // namespace Dal::AAD
