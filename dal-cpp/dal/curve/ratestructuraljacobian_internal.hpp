//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <string>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ratestructuraljacobian.hpp>

namespace Dal {
    struct RateStructuralJacobianDescriptor_::Data_ {
        size_t inputs_ = 0;
        Vector_<Vector_<size_t>> supports_;
        Vector_<RateCurveParameterCoordinate_> inputAxis_;
        Vector_<String_> outputAxis_;
        String_ reason_;
        std::string canonical_;
    };

    namespace RateStructuralJacobianInternal {
        class Canonical_ {
            std::string* bytes_;

        public:
            explicit Canonical_(std::string* bytes) : bytes_(bytes) {}
            void Add(size_t value);
            void Add(int value);
            void Add(bool value);
            void Add(double value);
            void Add(const String_& value);
            void Add(const char* value);
            void Add(const Date_& value);
            void Add(const DateTime_& value);
            template <class... T_> void Fields(const T_&... values) { (Add(values), ...); }
        };

        void AppendCurveDefinition(const CurveParameterState_& state, Canonical_* record);
        void AppendTrade(const RateTradeDefinition_& trade, Canonical_* record);
    } // namespace RateStructuralJacobianInternal
} // namespace Dal
