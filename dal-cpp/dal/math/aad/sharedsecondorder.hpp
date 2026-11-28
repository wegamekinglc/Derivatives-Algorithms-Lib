#pragma once

#include <memory>

#include <dal/math/aad/secondorder.hpp>

namespace Dal::AAD {
    namespace SharedSecondOrderDetail {
        struct Request_;
        struct Tape_;
    } // namespace SharedSecondOrderDetail

    // Active scalar for fixed, smooth, shared-graph kernels. A passive constant
    // may be copied freely. Active copies belong to one recording.
    class SharedSecondOrderNumber_ {
        double value_ = 0.0;
        std::uint32_t slot_ = 0;
        std::uint64_t identity_ = 0;

        SharedSecondOrderNumber_(double value, std::uint32_t slot, std::uint64_t identity)
            : value_(value), slot_(slot), identity_(identity) {}
        friend struct SharedSecondOrderDetail::Tape_;

    public:
        SharedSecondOrderNumber_() = default;
        explicit SharedSecondOrderNumber_(double value) : value_(value) {}
        SharedSecondOrderNumber_& operator+=(SharedSecondOrderNumber_ rhs);
        SharedSecondOrderNumber_& operator-=(SharedSecondOrderNumber_ rhs);
        SharedSecondOrderNumber_& operator*=(SharedSecondOrderNumber_ rhs);
        SharedSecondOrderNumber_& operator/=(SharedSecondOrderNumber_ rhs);
    };

    struct SharedSecondOrderExecution_ {
        String_ method_ = "NativeSharedPrimalSecondOrderAAD";
        size_t callbackEvaluations_ = 0;
        size_t sharedRecordings_ = 0;
        size_t sharedTangentSweeps_ = 0;
        size_t sharedReverseSweeps_ = 0;
        size_t replayDirections_ = 0;
        size_t pointwiseRecordings_ = 0;
        size_t pointwiseReverseSweeps_ = 0;
        size_t pointwiseSafePathActivations_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t retainedTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    class SharedSecondOrderResult_ {
        double value_ = 0.0;
        Vector_<> point_, gradient_, directional_;
        Matrix_<> directions_, products_;
        SharedSecondOrderExecution_ execution_;

        SharedSecondOrderResult_() = default;
        friend struct SharedSecondOrderDetail::Request_;

    public:
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return directions_; }
        [[nodiscard]] const Vector_<>& DirectionalDerivatives() const { return directional_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const SharedSecondOrderExecution_& Execution() const { return execution_; }
    };

    namespace SharedSecondOrderDetail {
        struct Kernel_ {
            const void* object_;
            SharedSecondOrderNumber_ (*shared_)(const void*, const Vector_<SharedSecondOrderNumber_>&);
            SecondOrderNumber_ (*pointwise_)(const void*, const Vector_<SecondOrderNumber_>&);
        };

        using WithKernel_ = void (*)(const void*, Request_&);
        void Run(Request_& request, const Kernel_& kernel);
        SharedSecondOrderResult_ Evaluate(const void* kernel, WithKernel_ own, const Vector_<>& point, const Matrix_<>& directions,
                                          const SecondOrderOptions_& options);
    } // namespace SharedSecondOrderDetail

    // The same deterministic C² kernel must support both active scalar
    // types and use the same fixed plan. A request needs at least two directions.
    // The kernel copy and any full pointwise replay run under one exclusive scope.
    template <class K_>
    [[nodiscard]] SharedSecondOrderResult_ EvaluateSharedSecondOrder(const K_& kernel,
                                                                     const Vector_<>& point,
                                                                     const Matrix_<>& directions,
                                                                     const SecondOrderOptions_& options = {}) {
        return SharedSecondOrderDetail::Evaluate(
            std::addressof(kernel),
            [](const void* source, SharedSecondOrderDetail::Request_& request) {
                const K_ owned(*static_cast<const K_*>(source));
                SharedSecondOrderDetail::Run(
                    request, {std::addressof(owned),
                              [](const void* object, const Vector_<SharedSecondOrderNumber_>& inputs) -> SharedSecondOrderNumber_ {
                                  return SharedSecondOrderNumber_((*static_cast<const K_*>(object))(inputs));
                              },
                              [](const void* object, const Vector_<SecondOrderNumber_>& inputs) -> SecondOrderNumber_ {
                                  return SecondOrderNumber_((*static_cast<const K_*>(object))(inputs));
                              }});
            },
            point, directions, options);
    }
} // namespace Dal::AAD

#include <dal/math/aad/detail/sharedsecondordertape.hpp>
