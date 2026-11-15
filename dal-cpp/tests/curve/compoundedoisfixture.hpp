#pragma once

#include <cmath>

#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescriptevaluation.hpp>
#include <dal/curve/yclogdf.hpp>

namespace Dal::CompoundedOisTest {
    inline CompoundedOisScriptSpec_ Contract(bool shortMaturity = false) {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = shortMaturity ? "ois-short" : "ois-long";
        s.notional_ = 100.;
        s.coupon_ = .02;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = DayBasis_("ACT_360");
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.calendar_ = HolidayCenterData_("explicit-Mar4", {Date_(2024, 3, 4)});
        const Date_ a(2024, 2, 29), b(2024, 3, 1), c(2024, 3, 5), d(2024, 3, 6), p(2024, 3, 8);
        s.fixedLeg_ = {{a, c, c, 5. / 365.}};
        s.coupons_ = {{"first", a, c, c}};
        s.observations_ = {{a, b, DateTime_(a), 1. / 360., "first", a, b}, {b, c, DateTime_(b), 4. / 360., "first", b, c}};
        if (!shortMaturity) {
            s.fixedLeg_.push_back({c, d, p, 1. / 365.});
            s.coupons_.push_back({"second", c, d, p});
            s.observations_.push_back({c, d, DateTime_(c), 1. / 360., "second", c, d});
        }
        return s;
    }

    inline Script::ScriptValuationSettings_ Valuation() {
        Script::ScriptValuationSettings_ v;
        v.evaluationDate_ = Date_(2024, 3, 1);
        v.fixings_.reset(new MarketFixingSnapshot_(
            {{"USD-SOFR", {{DateTime_(Date_(2024, 2, 29)), -.012}, {DateTime_(Date_(2024, 3, 1)), .9}, {DateTime_(Date_(2024, 3, 5)), .8}}}}));
        return v;
    }

    inline Vector_<> Parameters() { return {-.0003, -.002}; }
    inline Vector_<> BackgroundParameters() { return {-.0002, -.0014}; }

    inline double LogDiscount(const Date_& d, const Vector_<>& parameters) {
        const double days = d - Date_(2024, 3, 1);
        return days <= 4. ? parameters[0] * days / 4. : parameters[0] + (parameters[1] - parameters[0]) * (days - 4.) / 15.;
    }

    inline Vector_<> Oracle(const Vector_<>& projection, const Vector_<>& discount, bool shortMaturity = false) {
        const Date_ a(2024, 3, 1), b(2024, 3, 5), c(2024, 3, 6), p(2024, 3, 8);
        const double first = (1. - .012 / 360.) * std::exp(LogDiscount(a, projection) - LogDiscount(b, projection)) - 1.;
        double floating = first * std::exp(LogDiscount(b, discount));
        double annuity = 5. / 365. * std::exp(LogDiscount(b, discount));
        if (!shortMaturity) {
            floating += std::expm1(LogDiscount(b, projection) - LogDiscount(c, projection)) * std::exp(LogDiscount(p, discount));
            annuity += 1. / 365. * std::exp(LogDiscount(p, discount));
        }
        return {100. * (floating - .02 * annuity), floating / annuity};
    }

    inline CurveInstrumentCalibrationSpec_ Calibration(const String_& role) {
        CurveInstrumentCalibrationSpec_ spec;
        const bool discountTarget = role == "Discount", shared = role == "Shared";
        spec.geometry_.today_ = Date_(2024, 3, 1);
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 20)};
        spec.geometry_.calibrateDiscountCurve_ = discountTarget;
        spec.geometry_.targetOvernightRoute_ = discountTarget ? "" : "USD.SOFR";
        spec.valuation_ = Valuation();
        spec.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, discountTarget || shared},
                                {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        const auto background = BackgroundParameters();
        const Handle_<DiscountCurve_> curve(new DiscountLogDF_("passive", "USD", spec.geometry_.knotDates_, {0., background[0], background[1]},
                                                               DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        if (discountTarget)
            spec.market_.overnightCurves_["USD.SOFR"] = curve;
        else if (!shared)
            spec.market_.discountCurves_[CollateralType_("OIS")] = curve;
        const auto projection = discountTarget ? background : Parameters();
        const auto discount = discountTarget || shared ? Parameters() : background;
        for (const bool shortMaturity : {true, false}) {
            const auto tables = PrepareCompoundedOis(Contract(shortMaturity), spec.valuation_);
            spec.compoundedOis_.push_back(tables);
            const auto id = shortMaturity ? "short" : "long";
            const bool deposit = discountTarget && shortMaturity;
            const auto instrument = deposit
                                        ? NewDepositScriptInstrument(Deposit_(spec.geometry_.today_, Date_(2024, 3, 5), .02, DayBasis_("ACT_360")),
                                                                     {"discount-anchor", Ccy_("USD"), "USD.OIS", 100.})
                                        : NewCompoundedOisScriptInstrument(tables);
            const double quote = deposit ? std::expm1(-Parameters()[0]) / (4. / 360.) : Oracle(projection, discount, shortMaturity)[1];
            spec.observations_.push_back(NewCalibrationObservation(
                id, instrument, "ParRate", NewCalibrationMarketQuote(id, quote, CalibrationValueUnit_::Parse("Rate")), 1e-13));
        }
        auto pv = Contract();
        pv.instrumentId_ = "off-market-pv";
        pv.coupon_ = .031;
        spec.compoundedOis_.push_back(PrepareCompoundedOis(pv, spec.valuation_));
        spec.solver_.initialGuessPerNode_ = {-.00025, -.0018};
        return spec;
    }

    inline CurveScriptEvaluationRequest_ Request(const CurveInstrumentCalibrationSpec_& spec, size_t table = 1) {
        CurveScriptEvaluationRequest_ request;
        request.geometry_ = spec.geometry_;
        request.market_ = spec.market_;
        request.valuation_ = spec.valuation_;
        request.parameters_ = Parameters();
        request.compoundedOis_ = spec.compoundedOis_[table];
        return request;
    }
} // namespace Dal::CompoundedOisTest
