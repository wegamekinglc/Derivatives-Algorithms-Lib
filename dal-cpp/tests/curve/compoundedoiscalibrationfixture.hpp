#pragma once

#include "compoundedoisfixture.hpp"

namespace Dal::CompoundedOisCalibrationTest {
    inline CompoundedOisObservationConvention_ Convention(const String_& mode) {
        const bool shifted = mode == "Lookback" || mode == "ObservationShift";
        return {CompoundedOisObservationMode_(mode), shifted ? 1 : 0, mode == "Lockout" ? 1 : 0};
    }
    inline Vector_<Vector_<Date_>> Boundaries(bool shortMaturity) {
        Vector_<Vector_<Date_>> rows{{Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 6)}};
        if (!shortMaturity)
            rows.push_back({Date_(2024, 3, 6), Date_(2024, 3, 7), Date_(2024, 3, 8), Date_(2024, 3, 9), Date_(2024, 3, 11), Date_(2024, 3, 12)});
        return rows;
    }
    inline CompoundedOisScriptSpec_ Contract(bool shortMaturity, const String_& mode, bool legacy = false) {
        auto s = CompoundedOisTest::Contract(shortMaturity);
        s.calendar_ = HolidayCenterData_("explicit-Mar4-working9", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
        s.fixedLeg_.clear();
        s.coupons_.clear();
        s.observations_.clear();
        size_t ordinal = 0;
        for (const auto& dates : Boundaries(shortMaturity)) {
            const String_ coupon = ordinal++ ? "second" : "first";
            const auto payment = dates.back().AddDays(coupon == "second" ? 1 : 0);
            s.coupons_.push_back({coupon, dates.front(), dates.back(), payment});
            s.fixedLeg_.push_back({dates.front(), dates.back(), payment, (dates.back() - dates.front()) / 365.});
            if (legacy)
                for (size_t i = 1; i < dates.size(); ++i)
                    s.observations_.push_back(
                        {dates[i - 1], dates[i], DateTime_(dates[i - 1]), (dates[i] - dates[i - 1]) / 360., coupon, dates[i - 1], dates[i]});
        }
        if (!legacy) {
            s.ruleId_ = "dal.compounded-ois-cashflows/2";
            s.observationConvention_ = Convention(mode);
        }
        return s;
    }
    inline Vector_<> Parameters(bool zeroRate = false) {
        return zeroRate ? Vector_<>{.0003 * 365. / 4., .002 * 365. / 19.} : CompoundedOisTest::Parameters();
    }
    inline double LogDiscount(Date_ date, const Vector_<>& theta, bool zeroRate = false) {
        const double days = date - Date_(2024, 3, 1);
        if (!zeroRate)
            return CompoundedOisTest::LogDiscount(date, theta);
        const double first = -theta[0] * 4. / 365., second = -theta[1] * 19. / 365.;
        return days <= 4. ? first * days / 4. : first + (second - first) * (days - 4.) / 15.;
    }
    inline Date_ Previous(Date_ date) {
        static const std::map<Date_, Date_> PREVIOUS{{Date_(2024, 3, 1), Date_(2024, 2, 29)}, {Date_(2024, 3, 5), Date_(2024, 3, 1)},
                                                     {Date_(2024, 3, 6), Date_(2024, 3, 5)},  {Date_(2024, 3, 7), Date_(2024, 3, 6)},
                                                     {Date_(2024, 3, 8), Date_(2024, 3, 7)},  {Date_(2024, 3, 9), Date_(2024, 3, 8)},
                                                     {Date_(2024, 3, 11), Date_(2024, 3, 9)}, {Date_(2024, 3, 12), Date_(2024, 3, 11)}};
        return PREVIOUS.at(date);
    }
    inline double Rate(Date_ start, Date_ end, const Vector_<>& theta, bool zeroRate, const Script::ScriptValuationSettings_& valuation) {
        const bool history = start < *valuation.evaluationDate_ ||
                             (start == *valuation.evaluationDate_ && valuation.todayFixingPolicy_ == TodayFixingPolicy_("RequireHistorical"));
        return history ? *valuation.fixings_->Find("USD-SOFR", DateTime_(start))
                       : std::expm1(LogDiscount(start, theta, zeroRate) - LogDiscount(end, theta, zeroRate)) / ((end - start) / 360.);
    }
    // Manual business boundaries and products; no prepared schedule, source, provider or cashflow helpers.
    inline double Product(
        const Vector_<Date_>& dates, const String_& mode, const Vector_<>& theta, bool zeroRate, const Script::ScriptValuationSettings_& valuation) {
        double product = 1.;
        for (size_t i = 1; i < dates.size(); ++i) {
            auto start = dates[i - 1], end = dates[i];
            if (mode == "Lookback" || mode == "ObservationShift") {
                start = Previous(start);
                end = Previous(end);
            }
            if (mode == "Lockout" && i + 1 == dates.size()) {
                start = dates[i - 2];
                end = dates[i - 1];
            }
            const double rate = Rate(start, end, theta, zeroRate, valuation);
            const double weight = mode == "ObservationShift" ? end - start : dates[i] - dates[i - 1];
            product *= 1. + weight / 360. * rate;
        }
        return product;
    }
    inline Vector_<> Oracle(const CompoundedOisScriptSpec_& s,
                            const String_& role,
                            const Vector_<>& theta,
                            bool zeroRate,
                            const Script::ScriptValuationSettings_& valuation) {
        const bool discountTarget = role == "Discount", passiveDiscount = role == "Overnight";
        const auto background = CompoundedOisTest::BackgroundParameters();
        const auto& projection = discountTarget ? background : theta;
        const auto& discount = passiveDiscount ? background : theta;
        const String_ mode = s.observationConvention_ ? s.observationConvention_->mode_.String() : "None";
        double floating = 0., annuity = 0.;
        const auto periods = Boundaries(s.coupons_.size() == 1);
        for (size_t i = 0; i < periods.size(); ++i) {
            const double df = std::exp(LogDiscount(s.coupons_[i].payment_, discount, zeroRate && !passiveDiscount));
            floating += (Product(periods[i], mode, projection, zeroRate && !discountTarget, valuation) - 1.) * df;
            annuity += s.fixedLeg_[i].accrual_ * df;
        }
        return {s.notional_ * (floating - s.coupon_ * annuity), floating / annuity};
    }
    inline CurveInstrumentCalibrationSpec_ Spec(const String_& mode, const String_& role, bool mixed = false, bool zeroRate = false) {
        auto s = CompoundedOisTest::Calibration(role);
        s.compoundedOis_.clear();
        s.observations_.clear();
        if (zeroRate) {
            s.geometry_.parameterization_ = CurveParameterization_("ZERO_RATE");
            s.geometry_.knotDates_ = {Date_(2024, 3, 5), Date_(2024, 3, 20)};
        }
        const auto theta = Parameters(zeroRate);
        for (const bool shortMaturity : {true, false}) {
            const bool deposit = role == "Discount" && shortMaturity;
            const auto contract = Contract(shortMaturity, mode, mixed && shortMaturity);
            const auto tables = PrepareCompoundedOis(contract, s.valuation_);
            s.compoundedOis_.push_back(tables);
            const auto instrument = deposit ? NewDepositScriptInstrument(Deposit_(s.geometry_.today_, Date_(2024, 3, 5), .02, DayBasis_("ACT_360")),
                                                                         {"discount-anchor", Ccy_("USD"), "USD.OIS", 100.})
                                            : NewCompoundedOisScriptInstrument(tables);
            const double quote = deposit ? std::expm1(-LogDiscount(Date_(2024, 3, 5), theta, zeroRate)) / (4. / 360.)
                                         : Oracle(contract, role, theta, zeroRate, s.valuation_)[1];
            const String_ id = shortMaturity ? "short" : "long";
            s.observations_.push_back(NewCalibrationObservation(id, instrument, "ParRate",
                                                                NewCalibrationMarketQuote(id, quote, CalibrationValueUnit_::Parse("Rate")), 1e-13));
        }
        auto pv = Contract(false, mode);
        pv.instrumentId_ = "registered-pv";
        pv.coupon_ = .031;
        s.compoundedOis_.push_back(PrepareCompoundedOis(pv, s.valuation_));
        s.solver_.initialGuessPerNode_ = theta * .95;
        return s;
    }
    inline Vector_<> Measures(const CurveInstrumentCalibrationSpec_& s, const String_& role, const Vector_<>& theta, bool zeroRate) {
        Vector_<> result;
        for (size_t i = 0; i < 2; ++i)
            result.push_back(role == "Discount" && i == 0 ? std::expm1(-LogDiscount(Date_(2024, 3, 5), theta, zeroRate)) / (4. / 360.)
                                                          : Oracle(s.compoundedOis_[i]->Spec(), role, theta, zeroRate, s.valuation_)[1]);
        return result;
    }
} // namespace Dal::CompoundedOisCalibrationTest
