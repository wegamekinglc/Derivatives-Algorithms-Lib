//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <memory>

#include <dal/model/gsrslvpricing.hpp>

namespace Dal::GSRSLVPricingInternal {
    class PreparedPricer_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;

    public:
        PreparedPricer_(const GSRSLVModelData_& model, const Vector_<GSREuropeanOption_>& options, const GSRMonteCarloSettings_& settings);
        Vector_<GSRMonteCarloPrice_> Price(const GSRSLVModelData_& model) const;
        Matrix_<> Jacobian(const GSRSLVModelData_& model, const Vector_<String_>& labels) const;
    };
} // namespace Dal::GSRSLVPricingInternal
