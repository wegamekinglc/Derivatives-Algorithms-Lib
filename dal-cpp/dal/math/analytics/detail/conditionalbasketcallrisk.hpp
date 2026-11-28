#pragma once

#include <array>

#include <dal/math/aad/secondorder.hpp>

namespace Dal::ConditionalBasketDetail {
    struct Node_ {
        double z_, weight_;
        bool linear_;
    };

    // Independent one-year call on a two-asset arithmetic basket. The plan is
    // bound to its certified point and quadrature. It accepts one direction.
    class FixedConditionalBasketCall_ {
    public:
        using Nodes_ = std::array<Node_, 48>;
        struct Trace_ {
            std::array<double, 48> prices_{};
            size_t statements_ = 0, edges_ = 0;
        };

    private:
        double strike_, rate_, discount_;
        Nodes_ nodes_;
        Vector_<> point_;

        template <bool TraceNodes_> AAD::SecondOrderNumber_ Record(const Vector_<AAD::SecondOrderNumber_>& x, Trace_* trace) const;

    public:
        FixedConditionalBasketCall_(double strike, double rate, const Nodes_& nodes, const Vector_<>& point);
        [[nodiscard]] AAD::SecondOrderResult_
        Evaluate(const Vector_<>& point, const Matrix_<>& directions, const AAD::SecondOrderOptions_& options = {}, Trace_* trace = nullptr) const;
    };
} // namespace Dal::ConditionalBasketDetail
