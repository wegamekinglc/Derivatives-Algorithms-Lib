//
// Created by Codex on 2026/10/2.
//

#include <dal/model/factory.hpp>

#include "gsrperf.hpp"

void RunGSRPathCases() {
    using namespace Dal;
    const auto curve = GSRBenchmarkCurve();
    const Date_ today = curve->evaluationDate_;
    const Vector_<Date_> gDates{today, today.AddDays(120), today.AddDays(240)};
    const Vector_<Date_> hDates{today, today.AddDays(180)};
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", gDates, {0.02, 0.015, 0.025}, hDates, {1.0, 0.8}));
    auto legacy = CreateModel<double>(Handle_<ModelData_>(new GSRModelData_("rates", curve, vol)));
    RunGSRPathCase(legacy.get(), false, "GSR 1F bond (100K paths x 4 steps)");
    RunGSRPathCase(legacy.get(), true, "GSR 1F swap and Libor (10K paths x 4 steps)");
    for (int n : {2, 3}) {
        MultiFactorGSRVolSettings_ settings;
        settings.gKnotDates_ = gDates;
        settings.hKnotDates_ = hDates;
        settings.gValues_ = Matrix_<>(n, 3, 0.0);
        settings.hValues_ = Matrix_<>(n, 2, 0.0);
        settings.correlations_ = Matrix_<>(n, n, 0.3);
        for (int factor = 0; factor < n; ++factor) {
            settings.factorNames_.push_back("factor" + String::FromInt(factor));
            settings.correlations_(factor, factor) = 1.0;
            for (int knot = 0; knot < 3; ++knot)
                settings.gValues_(factor, knot) = vol->gValues_[knot] / (factor + 1);
            for (int knot = 0; knot < 2; ++knot)
                settings.hValues_(factor, knot) = vol->hValues_[knot] * (1.0 - factor * 0.6);
        }
        const Handle_<MultiFactorGSRVolData_> multiVol(new MultiFactorGSRVolData_("vol", settings));
        auto model = CreateModel<double>(Handle_<ModelData_>(new MultiFactorGSRModelData_("rates", curve, multiVol)));
        RunGSRPathCase(model.get(), false, "GSR " + std::to_string(n) + "F bond (100K paths x 4 steps)");
        RunGSRPathCase(model.get(), true, "GSR " + std::to_string(n) + "F swap and Libor (10K paths x 4 steps)");
    }
}

void RunGSRSLVPathCases() {
    using namespace Dal;
    const auto curve = GSRBenchmarkCurve();
    for (int n : {1, 2, 3}) {
        MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = vol.hKnotDates_ = {curve->evaluationDate_};
        vol.gValues_ = vol.hValues_ = Matrix_<>(n, 1);
        vol.correlations_ = Matrix_<>(n, n, 0.3);
        for (int factor = 0; factor < n; ++factor) {
            vol.factorNames_.push_back("factor" + String::FromInt(factor));
            vol.gValues_(factor, 0) = 0.02 / (factor + 1);
            vol.hValues_(factor, 0) = 1.0 - 0.6 * factor;
            vol.correlations_(factor, factor) = 1.0;
        }
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        Matrix_<> values(2, 1);
        values(0, 0) = 0.8;
        values(1, 0) = 1.2;
        const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("leverage", {-0.05, 0.05}, {0.0}, values));
        for (double step : {0.25, 1.0 / 52.0}) {
            GSRSLVSettings_ settings;
            settings.maxStep_ = step;
            settings.varianceCorrelations_ = Vector_<>(n, -0.2);
            auto model = CreateModel<double>(Handle_<ModelData_>(new GSRSLVModelData_("smile", gaussian, leverage, settings)));
            RunGSRPathCase(model.get(), false, "GSR SLV " + std::to_string(n) + "F bond (100K paths, maxStep=" + std::to_string(step) + ")");
        }
    }
}
