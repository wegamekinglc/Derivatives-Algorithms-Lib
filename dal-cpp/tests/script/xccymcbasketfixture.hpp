#pragma once

#include <dal/script/xccymcbasket_internal.hpp>

#include "xccymcfixture.hpp"

namespace Dal::TestXccyBasket {
    using namespace TestXccyMc;
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    inline Vector_<XccyMcBasketMember_> Members(const String_& measure = "Price") {
        Vector_<XccyMcBasketMember_> members;
        const Vector_<Date_> maturities{Date_(2027, 11, 4), Date_(2028, 5, 4), Date_(2028, 11, 4)};
        for (size_t i = 0; i < maturities.size(); ++i)
            members.push_back({"member-" + CalibrationInternal::OrdinalText(i),
                               Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, maturities[i]), measure});
        return members;
    }
    inline XccyMcSettings_ AadSettings() {
        auto settings = Settings(4, 3);
        settings.aad_ = settings.retainPaths_ = true;
        const auto model = Data(NonflatSpec());
        Vector_<String_> labels;
        static_cast<void>(XccyGaussianMomentNativeMap(*model, &labels));
        const String_ sigma = "sigma:EUR/USD:" + Date::ToString(model->dynamics_->bucketEndDates_.back());
        settings.parameterLabels_ = {labels[0], model->market_->basis_->nativeLabels_.back(), sigma};
        return settings;
    }
} // namespace Dal::TestXccyBasket
