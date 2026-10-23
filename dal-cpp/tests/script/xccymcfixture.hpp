#pragma once

#include "../curve/xccygaussianexecutionfixture.hpp"
#include <dal/calibration/validation_internal.hpp>
#include <dal/script/xccymc.hpp>

namespace Dal::TestXccyMc {
    inline XccyMcSettings_ Settings(int batches = 4, int paths = 8) {
        XccyMcSettings_ settings;
        settings.phase_ = XccyMonteCarloPhase_::Value_::Validation;
        settings.batches_ = batches;
        settings.pathsPerBatch_ = paths;
        return settings;
    }
    inline Handle_<XccyGaussianModelData_> Data(const XccyGaussianModelSpec_& spec) {
        return Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("mc-test", spec));
    }
    inline Handle_<XccyGaussianCurveData_> BumpCurve(const Handle_<XccyGaussianCurveData_>& curve, const String_& label, double h) {
        auto values = curve->nativeValues_;
        for (size_t j = 0; j < values.size(); ++j)
            if (curve->nativeLabels_[j] == label)
                values[j] += h;
        Vector_<> logDF(curve->logDF_.size(), 0.);
        for (size_t i = 0; i < logDF.size(); ++i)
            for (size_t j = 0; j < values.size(); ++j)
                logDF[i] += curve->snapshotJacobian_(static_cast<int>(i), static_cast<int>(j)) * values[j];
        return Handle_<XccyGaussianCurveData_>(new XccyGaussianCurveData_("bump", curve->currency_, curve->route_, curve->nodeDates_, logDF,
                                                                          curve->nativeLabels_, values, curve->snapshotJacobian_));
    }
    inline Handle_<XccyGaussianModelData_> Bump(const XccyGaussianModelData_& data, const String_& label, double h) {
        const auto& m = *data.market_;
        Vector_<Handle_<XccyGaussianCurveData_>> projections;
        for (const auto& curve : m.projections_)
            projections.push_back(BumpCurve(curve, label, h));
        const double spot = m.fxSpot_ + (label == "fxspot:EUR/USD" ? h : 0.);
        const Handle_<XccyGaussianMarket_> market(new XccyGaussianMarket_(
            "bump", m.valuationTime_.Date(), m.pair_.domestic_.String(), m.pair_.foreign_.String(), m.collateralCurrency_.String(), spot,
            m.discountRoute_, BumpCurve(m.domestic_, label, h), BumpCurve(m.foreign_, label, h), BumpCurve(m.basis_, label, h), projections));
        const auto& d = *data.dynamics_;
        std::array<Vector_<>, 3> sigmas{d.domesticSigma_, d.foreignSigma_, d.fxSigma_};
        const std::array<String_, 3> names{"USD", "EUR", "EUR/USD"};
        for (size_t driver = 0; driver < sigmas.size(); ++driver)
            for (size_t bucket = 0; bucket < sigmas[driver].size(); ++bucket)
                if (label == "sigma:" + names[driver] + ":" + Date::ToString(d.bucketEndDates_[bucket]))
                    sigmas[driver][bucket] += h;
        const Handle_<XccyGaussianDynamics_> dynamics(new XccyGaussianDynamics_("bump", d.domesticA_, d.foreignA_, d.bucketEndDates_, sigmas[0],
                                                                                sigmas[1], sigmas[2], d.factorNames_, d.correlation_));
        return Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("bump", market, dynamics));
    }
    inline double Scale(const String_& label) {
        const auto bytes = CalibrationInternal::Bytes(label);
        return bytes.rfind("fxspot:", 0) == 0 || bytes.rfind("sigma:EUR/USD:", 0) == 0 ? .01 : .001;
    }
    inline long double
    WidePrice(const AAD::XccyGaussian_<long double>& model, const Vector_<AAD::XccyGaussianState_<long double>>& states, bool foreignSpread = true) {
        const auto state = [&](const Date_& date) -> const AAD::XccyGaussianState_<long double>& {
            return *std::find_if(states.begin(), states.end(), [&](const auto& value) { return value.date_ == date; });
        };
        const Date_ start(2027, 5, 4), reset(2027, 8, 4), end(2027, 11, 4), delayed(2027, 11, 8), firstFix(2027, 4, 30), secondFix(2027, 8, 2);
        const long double foreignN = 110.L * static_cast<long double>(100. / 110.);
        const long double later = foreignN * model.Spot(state(reset));
        const auto coupon = [&](const String_& currency, const String_& tenor, const Date_& fix, const Date_& from, const Date_& to) {
            const double alpha = (to - from) / 360.;
            return static_cast<long double>(alpha) * model.ForwardRate(currency, tenor, state(fix), from, to, alpha);
        };
        const long double domesticSpread = foreignSpread ? 0.L : .001L, foreignSpreadValue = foreignSpread ? .001L : 0.L;
        long double result = 110.L *
                             (coupon("USD", "3M", firstFix, start, reset) + domesticSpread * static_cast<long double>((reset - start) / 360.)) /
                             model.DomesticNumeraire(state(reset));
        result += later * (coupon("USD", "3M", secondFix, reset, end) + domesticSpread * static_cast<long double>((end - reset) / 360.)) /
                  model.DomesticNumeraire(state(end));
        result -= foreignN * (coupon("EUR", "6M", firstFix, start, end) + foreignSpreadValue * static_cast<long double>((end - start) / 360.)) *
                  model.Spot(state(delayed)) / model.DomesticNumeraire(state(delayed));
        result += (-110.L + foreignN * model.Spot(state(start))) / model.DomesticNumeraire(state(start));
        result += (later - foreignN * model.Spot(state(end))) / model.DomesticNumeraire(state(end));
        result += (later - 110.L) / model.DomesticNumeraire(state(reset));
        return (foreignSpread ? 1.L : -1.L) * result;
    }
} // namespace Dal::TestXccyMc
