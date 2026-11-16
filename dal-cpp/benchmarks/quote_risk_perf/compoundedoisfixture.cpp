#include <dal/platform/platform.hpp>

#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/time/dateutils.hpp>

#include "compoundedoisfixture.hpp"

namespace {
    using namespace Dal;
    const Date_ TODAY(2024, 3, 1);
    bool Business(Date_ date) { return date == Date_(2024, 3, 9) || (!Date::IsWeekEnd(date) && date != Date_(2024, 3, 4)); }
    Vector_<Date_> Axis(int factors) {
        Vector_<Date_> result{Date_(2024, 2, 29), TODAY};
        while (result.size() < static_cast<size_t>(factors + 2)) {
            auto next = result.back().AddDays(1);
            while (!Business(next))
                next = next.AddDays(1);
            result.push_back(next);
        }
        return result;
    }
    double LogDiscount(Date_ date, const Vector_<Date_>& nodes, const Vector_<>& theta) {
        const auto right = std::lower_bound(nodes.begin() + 1, nodes.end(), date);
        REQUIRE(right != nodes.end(), "Oracle date outside native knots");
        const size_t j = right - nodes.begin();
        const double leftValue = j == 1 ? 0. : theta[j - 2];
        return leftValue + (theta[j - 1] - leftValue) * (date - nodes[j - 1]) / (nodes[j] - nodes[j - 1]);
    }
    double Product(const Vector_<Date_>& axis, int first, int count, const String_& mode, const Vector_<Date_>& nodes, const Vector_<>& theta) {
        double product = 1.;
        for (int i = first + 1; i <= count; ++i) {
            const int sample = mode == "ObservationShift" ? i - 1 : mode == "Lockout" && i > count - 5 ? count - 5 : i;
            const double rateDays = axis[sample + 1] - axis[sample];
            const double rate =
                sample == 0 ? .02
                            : std::expm1(LogDiscount(axis[sample], nodes, theta) - LogDiscount(axis[sample + 1], nodes, theta)) * 360. / rateDays;
            const double weight = mode == "ObservationShift" ? rateDays : axis[i + 1] - axis[i];
            product *= 1. + weight / 360. * rate;
        }
        return product;
    }
    CompoundedOisScriptSpec_ Contract(Date_ end, const String_& mode, const String_& id) {
        CompoundedOisScriptSpec_ contract;
        contract.instrumentId_ = id;
        contract.direction_ = "PayFixed";
        contract.notional_ = 1.;
        contract.coupon_ = .03;
        contract.currency_ = contract.collateralCurrency_ = Ccy_("USD");
        contract.collateralType_ = CollateralType_("OIS");
        contract.discountRoute_ = "USD.OIS";
        contract.projectionRoute_ = "USD.SOFR";
        contract.fixingIndex_ = "USD-SOFR";
        contract.observationBasis_ = DayBasis_("ACT_360");
        contract.fixedBasis_ = DayBasis_("ACT_365F");
        contract.ruleId_ = "dal.compounded-ois-cashflows/2";
        contract.observationConvention_ =
            CompoundedOisObservationConvention_{CompoundedOisObservationMode_(mode), mode == "ObservationShift" ? 1 : 0, mode == "Lockout" ? 5 : 0};
        contract.calendar_ = HolidayCenterData_("Mar4-holiday-Mar9-working", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
        contract.coupons_ = {{"coupon", TODAY, end, end}};
        contract.fixedLeg_ = {{TODAY, end, end, (end - TODAY) / 365.}};
        return contract;
    }
    double Quote(const CompoundedOisScriptSpec_& contract,
                 const Vector_<Date_>& axis,
                 const Vector_<Date_>& nodes,
                 const Vector_<>& theta,
                 const String_& mode) {
        double floating = 0., annuity = 0.;
        for (size_t i = 0; i < contract.coupons_.size(); ++i) {
            const auto& coupon = contract.coupons_[i];
            const int first = static_cast<int>(std::lower_bound(axis.begin(), axis.end(), coupon.start_) - axis.begin()) - 1;
            const int last = static_cast<int>(std::lower_bound(axis.begin(), axis.end(), coupon.end_) - axis.begin()) - 1;
            const double df = std::exp(-.015 * (coupon.payment_ - TODAY) / 365.);
            floating += (Product(axis, first, last, mode, nodes, theta) - 1.) * df;
            annuity += contract.fixedLeg_[i].accrual_ * df;
        }
        return floating / annuity;
    }
    CompoundedOisRiskCase_ Case(int quotes, int pvs, int factors, const String_& mode) {
        CompoundedOisRiskCase_ result;
        result.pvs_ = pvs;
        result.factors_ = factors;
        result.mode_ = mode;
        auto& spec = result.spec_;
        spec.geometry_.today_ = TODAY;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetOvernightRoute_ = "USD.SOFR";
        spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        spec.geometry_.knotDates_ = {TODAY};
        spec.valuation_.evaluationDate_ = TODAY;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 2, 29)), .02}}}}));
        spec.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, false}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        const auto axis = Axis(factors);
        Vector_<> theta;
        for (int i = 1; i <= quotes; ++i) {
            const auto end = axis[1 + factors * i / quotes];
            spec.geometry_.knotDates_.push_back(end);
            theta.push_back(-(.02 + .0005 * i) * (end - TODAY) / 365.);
        }
        const auto last = spec.geometry_.knotDates_.back();
        spec.market_.discountCurves_[CollateralType_("OIS")].reset(new DiscountLogDF_(
            "passive-discount", "USD", {TODAY, last}, {0., -.015 * (last - TODAY) / 365.}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        for (int i = 1; i <= quotes; ++i) {
            const auto id = String::FromInt(i);
            auto contract = Contract(spec.geometry_.knotDates_[i], mode, "ois" + id);
            contract.coupons_.clear();
            contract.fixedLeg_.clear();
            for (int c = 0; c < i; ++c) {
                const auto start = spec.geometry_.knotDates_[c], end = spec.geometry_.knotDates_[c + 1];
                contract.coupons_.push_back({"coupon" + String::FromInt(c), start, end, end});
                contract.fixedLeg_.push_back({start, end, end, (end - start) / 365.});
            }
            const auto table = PrepareCompoundedOis(contract, spec.valuation_);
            const double quote = Quote(contract, axis, spec.geometry_.knotDates_, theta, mode);
            spec.compoundedOis_.push_back(table);
            spec.observations_.push_back(NewCalibrationObservation("o" + id, NewCompoundedOisScriptInstrument(table), "ParRate",
                                                                   NewCalibrationMarketQuote("q" + id, quote, CalibrationValueUnit_::Parse("Rate")),
                                                                   1e-13));
        }
        if (quotes == 15) {
            for (int i = 0; i < quotes; ++i)
                theta[i] *= -365. / (spec.geometry_.knotDates_[i + 1] - TODAY);
            spec.geometry_.parameterization_ = CurveParameterization_("ZERO_RATE");
            spec.geometry_.knotDates_.erase(spec.geometry_.knotDates_.begin());
        }
        spec.solver_.initialGuessPerNode_ = theta * .95;
        return result;
    }
} // namespace

Dal::Vector_<CompoundedOisRiskCase_> CompoundedOisRiskCases() {
    return {Case(5, 1, 30, "None"), Case(15, 1, 30, "ObservationShift"), Case(15, 100, 30, "ObservationShift"), Case(2, 1, 365, "Lockout"),
            Case(2, 1, 1825, "Lockout")};
}
