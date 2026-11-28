#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration SecondOrderErrorReason
    Failure reason for an independent second-order AAD request
switchable
alternative INVALID_INPUT
alternative OUTSIDE_DOMAIN
alternative UNSUPPORTED_OPERATION
alternative BUDGET_EXCEEDED
alternative CAPACITY_EXCEEDED
alternative LIFECYCLE
alternative NUMERICAL_FAILURE
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration SecondOrderStage
    Phase at which a second-order AAD request failed
switchable
alternative VALIDATION
alternative RECORDING
alternative REVERSE
alternative RESULT
alternative CLEANUP
-IF-------------------------------------------------------------------------*/

namespace Dal::AAD {
#include <dal/auto/MG_SecondOrderErrorReason_enum.hpp>
#include <dal/auto/MG_SecondOrderStage_enum.hpp>

    namespace SecondOrderDetail {
        struct Expression_ {};
        struct Access_;
        template <class E_> using EnableExpression_ = std::enable_if_t<std::is_base_of_v<Expression_, std::decay_t<E_>>, int>;
    } // namespace SecondOrderDetail

    class SecondOrderNumber_ : public SecondOrderDetail::Expression_ {
        double value_ = 0.0;
        double tangent_ = 0.0;
        std::uint64_t arenaId_ = 0;
        std::uint32_t generation_ = 0, slot_ = 0;

        friend struct SecondOrderDetail::Access_;

    public:
        SecondOrderNumber_() = default;
        explicit SecondOrderNumber_(double value) : value_(value) {}

        template <class E_, SecondOrderDetail::EnableExpression_<E_> = 0> SecondOrderNumber_(const E_& expression);
        template <class E_> SecondOrderNumber_& operator+=(const E_& rhs);
        template <class E_> SecondOrderNumber_& operator-=(const E_& rhs);
        template <class E_> SecondOrderNumber_& operator*=(const E_& rhs);
        template <class E_> SecondOrderNumber_& operator/=(const E_& rhs);
    };

    using SecondOrderFunction_ = std::function<SecondOrderNumber_(const Vector_<SecondOrderNumber_>&)>;

    struct SecondOrderOptions_ {
        std::optional<size_t> numericPayloadBudgetBytes_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    struct SecondOrderExecution_ {
        String_ method_ = "NativePointwiseSecondOrderAAD";
        size_t callbackEvaluations_ = 0;
        size_t recordings_ = 0;
        size_t reverseSweeps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t retainedTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
        size_t safePathActivations_ = 0;
    };

    struct SecondOrderCapabilities_ {
        bool smoothDirectionalProducts_ = true;
        bool independentNesting_ = false;
        bool reverseEvents_ = false;
        bool checkpoints_ = false;
        bool nonsmoothOperators_ = false;
    };

    [[nodiscard]] constexpr SecondOrderCapabilities_ SecondOrderCapabilities() noexcept { return {}; }

    class SecondOrderError_ : public Exception_ {
        SecondOrderErrorReason_ reason_;
        SecondOrderStage_ stage_;
        std::optional<size_t> direction_;

        SecondOrderError_(SecondOrderErrorReason_ reason, SecondOrderStage_ stage, std::optional<size_t> direction, const std::string& message);
        friend struct SecondOrderDetail::Access_;

    public:
        [[nodiscard]] SecondOrderErrorReason_ Reason() const { return reason_; }
        [[nodiscard]] SecondOrderStage_ Stage() const { return stage_; }
        [[nodiscard]] std::optional<size_t> DirectionIndex() const { return direction_; }
    };

    class SecondOrderResult_ {
        double value_ = 0.0;
        Vector_<> point_, gradient_, directional_;
        Matrix_<> directions_, products_;
        SecondOrderExecution_ execution_;

        friend SecondOrderResult_ EvaluateSecondOrder(const SecondOrderFunction_&, const Vector_<>&, const Matrix_<>&, const SecondOrderOptions_&);
        friend struct SecondOrderDetail::Access_;
        SecondOrderResult_() = default;

    public:
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return directions_; }
        [[nodiscard]] const Vector_<>& DirectionalDerivatives() const { return directional_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const SecondOrderExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] size_t SecondOrderPayloadBytes(size_t inputs, size_t directions);
    [[nodiscard]] SecondOrderResult_ EvaluateSecondOrder(const SecondOrderFunction_& function,
                                                         const Vector_<>& point,
                                                         const Matrix_<>& directions,
                                                         const SecondOrderOptions_& options = {});
} // namespace Dal::AAD

#include <dal/math/aad/detail/secondorderexpression.hpp>
