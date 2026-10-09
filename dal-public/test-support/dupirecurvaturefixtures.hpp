//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/dupireriskrequest.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/ivs.hpp>

namespace Dal::Script::TestSupport::DupireCurvature {
    struct SingleWorker_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        SingleWorker_() { pool_->Start(1, true); }
        ~SingleWorker_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    class FlatIVS_ final : public Dal::AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    inline auto Calibration() {
        const Dal::DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, 0.001), {60.0, 100.0, 140.0}, 10.0,
                                            {0.5, 1.0},           0.5};
        return Dal::CalibrateDupireWithRisk(FlatIVS_(), inputs, "curvature");
    }

    inline auto Model(const Dal::DupireCalibrationSnapshot_& calibration) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::NewHybridLocalVolEquityData("Z_LOCAL", "EQ[LOCAL]", "USD", "F_LOCAL", calibration.Spot(),
                                                                 calibration.DividendYield(), calibration.Surface(), 0.25),
                                Dal::NewHybridDeterministicRateData("00_RATE", "USD", calibration.Rate()),
                                Dal::NewHybridBSEquityData("A_OTHER", "EQ[OTHER]", "USD", "F_OTHER", 120.0, 0.25, 0.01)};
        Dal::Matrix_<> correlation(2, 2, 0.0);
        correlation(0, 0) = correlation(1, 1) = 1.0;
        settings.correlation_ = Dal::NewHybridConstantCorrelationData("correlation", {"F_LOCAL", "F_OTHER"}, correlation);
        return Dal::NewHybridModelData("unsorted", settings);
    }

    inline auto Product() {
        return Dal::NewScriptProduct("curvature", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                     {"0.001", "pay PAYS FIX(EQ[LOCAL]) + 0.1 * FIX(EQ[OTHER]) + QUOTE * QUOTE"});
    }

    inline auto RiskRequest() {
        Dal::DupireScriptRiskRequest_ request;
        request.numPaths_ = 17;
        request.directBindings_ = {{0, "quote:3"}};
        request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
        request.simulation_.compiled_ = false;
        return request;
    }

    inline Dal::String_ Number(double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return Dal::String_(out.str());
    }

    inline auto MixedProduct(double quote) {
        return Dal::NewScriptProduct("mixed", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                     {Number(quote), "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 0.7 * QUOTE * QUOTE"
                                                     " + 2 * QUOTE * FIX(EQ[LOCAL]) + 0.1 * FIX(EQ[OTHER])"});
    }

} // namespace Dal::Script::TestSupport::DupireCurvature
