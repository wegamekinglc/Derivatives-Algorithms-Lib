//
// Created by GitHub Copilot on 2026/5/19.
//

#include <iomanip>
#include <iostream>
#include <memory>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/schedules.hpp>

using namespace Dal;


namespace {
    Handle_<DiscountCurve_> MakeFlatDiscountCurve(const String_& name,
                                                  const String_& ccy,
                                                  const Date_& today,
                                                  double rate,
                                                  const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        const Vector_<Date_> knotDates = {
            Date::AddMonths(today, 1),
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
            Date::AddMonths(today, 36),
            Date::AddMonths(today, 60),
            Date::AddMonths(today, 84),
            Date::AddMonths(today, 120),
        };
        const Vector_<> values(knotDates.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, values, values), base));
    }

    Handle_<YCInstrument_> QuotedInstrument(const Handle_<YCInstrument_>& prototype,
                                           const YieldCurve_& marketCurve,
                                           const Date_& tradeDate,
                                           const Ccy_& ccy) {
        const auto rate = prototype->Precompute(Handle_<YieldCurve_>());

        if (auto deposit = dynamic_cast<const Deposit_*>(prototype.get())) {
            const auto span = deposit->TimeSpan();
            return Handle_<YCInstrument_>(new Deposit_(tradeDate,
                                                       span.first,
                                                       span.second,
                                                       (*rate)(marketCurve),
                                                       Ccy::Conventions::OisIndex()(ccy)));
        }
        if (auto fra = dynamic_cast<const FRA_*>(prototype.get())) {
            const auto span = fra->TimeSpan();
            return Handle_<YCInstrument_>(
                new FRA_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::LiborIndex()(ccy)));
        }
        if (auto swap = dynamic_cast<const OISSwap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
            auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
            auto floatLeg = fixedLeg;
            floatLeg.paymentFrequency_ = PeriodLength_("12M");
            floatLeg.dayBasis_ = overnightIndex.dayBasis_;
            return Handle_<YCInstrument_>(
                new OISSwap_(tradeDate, span.first, span.second, (*rate)(marketCurve), fixedLeg, overnightIndex, floatLeg));
        }
        if (auto swap = dynamic_cast<const Swap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            return Handle_<YCInstrument_>(new Swap_(tradeDate,
                                                    span.first,
                                                    span.second,
                                                    (*rate)(marketCurve),
                                                    Ccy::Conventions::SwapFixedLeg()(ccy),
                                                    Ccy::Conventions::LiborIndex()(ccy),
                                                    Ccy::Conventions::SwapFloatLeg()(ccy)));
        }
        REQUIRE(false, "Unsupported example instrument type");
        return Handle_<YCInstrument_>();
    }

    void PrintScheduleExample(const Date_& today, const Ccy_& ccy) {
        auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
        fixedLeg.accrualHolidays_ = Holidays::None();
        fixedLeg.paymentHolidays_ = Holidays::None();

        const auto periods = MakeSchedulePeriods(today,
                                                 Date::AddMonths(today, 24),
                                                 fixedLeg.paymentFrequency_,
                                                 fixedLeg.accrualHolidays_,
                                                 0,
                                                 Holidays::None(),
                                                 fixedLeg.paymentLag_,
                                                 fixedLeg.paymentHolidays_,
                                                 DateGeneration_("Forward"),
                                                 fixedLeg.businessDayConvention_,
                                                 fixedLeg.paymentConvention_,
                                                 fixedLeg.endOfMonth_);

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Convention-based schedule example\n"
                  << std::string(70, '=') << "\n\n";
        const Vector_<int> w = {14, 14, 14, 10};
        std::cout << std::left  << std::setw(w[0]) << "AccrualStart"
                  << std::setw(w[1]) << "AccrualEnd"
                  << std::setw(w[2]) << "PaymentDate"
                  << std::right << std::setw(w[3]) << "Stub" << '\n';
        std::cout << std::string(52, '-') << '\n';
        for (const auto& period : periods) {
            std::cout << std::left  << std::setw(w[0]) << Date::ToString(period.accrualStart_)
                      << std::setw(w[1]) << Date::ToString(period.accrualEnd_)
                      << std::setw(w[2]) << Date::ToString(period.paymentDate_)
                      << std::right << std::setw(w[3]) << (period.isStub_ ? "yes" : "no") << '\n';
        }
        std::cout << '\n';
    }

    void PrintScheduleContextExample() {
        const Date_ start(2024, 1, 31);
        const Date_ maturity(2024, 7, 31);
        const Holidays_ hols(Holidays::None());
        const auto periods = MakeSchedulePeriods(start,
                                                 maturity,
                                                 PeriodLength_("3M"),
                                                 hols,
                                                 2,
                                                 hols,
                                                 2,
                                                 hols,
                                                 DateGeneration_("Forward"),
                                                 BizDayConvention_("ModifiedFollowing"),
                                                 BizDayConvention_("ModifiedFollowing"),
                                                 true);

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Schedule context example with fixing/payment lags\n"
                  << std::string(70, '=') << "\n\n";
        const Vector_<int> w = {14, 14, 14, 10, 10};
        std::cout << std::left  << std::setw(w[0]) << "FixingDate"
                  << std::setw(w[1]) << "AccrualEnd"
                  << std::setw(w[2]) << "PaymentDate"
                  << std::right << std::setw(w[3]) << "Months"
                  << std::setw(w[4]) << "Stub" << '\n';
        std::cout << std::string(62, '-') << '\n';
        for (const auto& period : periods) {
            std::cout << std::left  << std::setw(w[0]) << Date::ToString(period.fixingDate_)
                      << std::setw(w[1]) << Date::ToString(period.accrualEnd_)
                      << std::setw(w[2]) << Date::ToString(period.paymentDate_)
                      << std::right << std::setw(w[3])
                      << (period.dayCountContext_ ? period.dayCountContext_->couponMonths_ : 0)
                      << std::setw(w[4]) << (period.isStub_ ? "yes" : "no") << '\n';
        }
        std::cout << '\n';
    }

    void PrintForwardInstrumentExample(const Date_& today, const Ccy_& ccy) {
        const String_ ccyName = ccy.String();
        const DayBasis_ basis("ACT_360");
        const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois", ccyName, today, 0.01);
        const Handle_<DiscountCurve_> libor3m = MakeFlatDiscountCurve("libor3m", ccyName, today, 0.03, ois);
        const Handle_<DiscountCurve_> libor6m = MakeFlatDiscountCurve("libor6m", ccyName, today, 0.035, ois);
        const CurveBlock_ curve("bundle",
                                ccyName,
                                {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                {{PeriodLength_("3M"), libor3m}, {PeriodLength_("6M"), libor6m}},
                                basis);

        auto libor3mIndex = Ccy::Conventions::LiborIndex()(ccy);
        libor3mIndex.accrualHolidays_ = Holidays::None();
        libor3mIndex.fixingHolidays_ = Holidays::None();
        libor3mIndex.dayBasis_ = basis;
        libor3mIndex.useProjectionCurve_ = true;
        libor3mIndex.forecastTenor_ = PeriodLength_("3M");
        libor3mIndex.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

        auto libor6mIndex = libor3mIndex;
        libor6mIndex.forecastTenor_ = PeriodLength_("6M");

        RateLegConvention_ spreadLeg;
        spreadLeg.paymentFrequency_ = PeriodLength_("3M");
        spreadLeg.dayBasis_ = basis;
        spreadLeg.accrualHolidays_ = Holidays::None();
        spreadLeg.paymentHolidays_ = Holidays::None();

        RateLegConvention_ referenceLeg = spreadLeg;
        referenceLeg.paymentFrequency_ = PeriodLength_("6M");

        const Handle_<YCInstrument_> future(new Future_(today,
                                                        Date::AddMonths(today, 3),
                                                        Date::AddMonths(today, 6),
                                                        0.0,
                                                        libor3mIndex,
                                                        0.0015));
        const Handle_<YCInstrument_> basisSwap(new BasisSwap_(today,
                                                              today,
                                                              Date::AddMonths(today, 24),
                                                              0.0,
                                                              libor3mIndex,
                                                              spreadLeg,
                                                              libor6mIndex,
                                                              referenceLeg));

        const auto futureRate = future->Precompute(Handle_<YieldCurve_>());
        const auto basisSwapRate = basisSwap->Precompute(Handle_<YieldCurve_>());

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Forward instrument routing example\n"
                  << std::string(70, '=') << "\n\n";
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  3M future rate with 15bp convexity adjustment: "
                  << std::setw(10) << std::right << (*futureRate)(curve) * 100.0 << "%\n";
        std::cout << "  3M vs 6M basis swap spread (separate forward curves): "
                  << std::setw(10) << std::right << (*basisSwapRate)(curve) * 10000.0 << " bp\n\n";
    }

    void PrintStageDiagnostics(const CurveCalibrationDiagnostics_& diagnostics,
                               const Vector_<String_>& names,
                               double elapsedMs) {
        std::cout << "\n  " << diagnostics.curveName_ << " calibration residuals";
        std::cout << "  (elapsed: " << int(elapsedMs) << " ms)\n";
        std::cout << "  " << std::string(36, '-') << "\n\n";
        const Vector_<int> w = {26, 12, 12, 12};
        std::cout << std::left  << std::setw(w[0]) << "Instrument"
                  << std::right << std::setw(w[1]) << "Market(%)"
                  << std::setw(w[2]) << "Model(%)"
                  << std::setw(w[3]) << "Error(bp)" << '\n';
        std::cout << std::string(62, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(diagnostics.instrumentNames_.size()); ++i) {
            std::cout << std::left  << std::setw(w[0])
                      << (static_cast<size_t>(i) < names.size() ? names[i].c_str() : diagnostics.instrumentNames_[i].c_str())
                      << std::right << std::setw(w[1]) << diagnostics.marketRates_[i] * 100.0
                      << std::setw(w[2]) << diagnostics.modelRates_[i] * 100.0
                      << std::setw(w[3]) << diagnostics.residuals_[i] * 10000.0 << '\n';
        }
        std::cout << '\n';
    }

    void PrintMultiCurveExample(const Date_& today, const Ccy_& ccy) {
        const String_ ccyName = ccy.String();
        auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
        fixedLeg.accrualHolidays_ = Holidays::None();
        fixedLeg.paymentHolidays_ = Holidays::None();

        auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
        overnightIndex.accrualHolidays_ = Holidays::None();
        overnightIndex.fixingHolidays_ = Holidays::None();

        auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
        libor3m.accrualHolidays_ = Holidays::None();
        libor3m.fixingHolidays_ = Holidays::None();

        auto floatLeg = Ccy::Conventions::SwapFloatLeg()(ccy);
        floatLeg.accrualHolidays_ = Holidays::None();
        floatLeg.paymentHolidays_ = Holidays::None();

        auto overnightLeg = fixedLeg;
        overnightLeg.paymentFrequency_ = PeriodLength_("12M");
        overnightLeg.dayBasis_ = overnightIndex.dayBasis_;

        const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois_market", ccyName, today, 0.01);
        const Handle_<DiscountCurve_> forward3m = MakeFlatDiscountCurve("libor3m_market", ccyName, today, 0.03, ois);
        const CurveBlock_ marketCurve("market",
                                      ccyName,
                                      {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                      {{libor3m.forecastTenor_, forward3m}},
                                      libor3m.dayBasis_);

        // ---- OIS stage: 20 instruments ----
        const Handle_<YCInstrument_> oisDep1w (new Deposit_(today, today, today.AddDays(7),              0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep2w (new Deposit_(today, today, today.AddDays(14),             0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep1m (new Deposit_(today, today, Date::AddMonths(today, 1),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep2m (new Deposit_(today, today, Date::AddMonths(today, 2),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep3m (new Deposit_(today, today, Date::AddMonths(today, 3),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep4m (new Deposit_(today, today, Date::AddMonths(today, 4),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep5m (new Deposit_(today, today, Date::AddMonths(today, 5),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep6m (new Deposit_(today, today, Date::AddMonths(today, 6),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep9m (new Deposit_(today, today, Date::AddMonths(today, 9),     0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep12m(new Deposit_(today, today, Date::AddMonths(today, 12),    0.0, overnightIndex));
        const Handle_<YCInstrument_> oisSwap18m(new OISSwap_(today, today, Date::AddMonths(today, 18),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap2y (new OISSwap_(today, today, Date::AddMonths(today, 24),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap30m(new OISSwap_(today, today, Date::AddMonths(today, 30),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap3y (new OISSwap_(today, today, Date::AddMonths(today, 36),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap4y (new OISSwap_(today, today, Date::AddMonths(today, 48),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap5y (new OISSwap_(today, today, Date::AddMonths(today, 60),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap6y (new OISSwap_(today, today, Date::AddMonths(today, 72),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap7y (new OISSwap_(today, today, Date::AddMonths(today, 84),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap8y (new OISSwap_(today, today, Date::AddMonths(today, 96),   0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap10y(new OISSwap_(today, today, Date::AddMonths(today, 120),  0.0, fixedLeg, overnightIndex, overnightLeg));
        const Vector_<String_> oisNames = {
            "OIS Deposit 1W", "OIS Deposit 2W", "OIS Deposit 1M", "OIS Deposit 2M",
            "OIS Deposit 3M", "OIS Deposit 4M", "OIS Deposit 5M", "OIS Deposit 6M",
            "OIS Deposit 9M", "OIS Deposit 12M",
            "OIS Swap 18M", "OIS Swap 2Y", "OIS Swap 30M", "OIS Swap 3Y", "OIS Swap 4Y",
            "OIS Swap 5Y", "OIS Swap 6Y", "OIS Swap 7Y", "OIS Swap 8Y", "OIS Swap 10Y",
        };

        // ---- Libor 3M stage: 20 instruments ----
        const Handle_<YCInstrument_> fra1x4  (new FRA_(today, Date::AddMonths(today, 1),  Date::AddMonths(today, 4),  0.0, libor3m));
        const Handle_<YCInstrument_> fra2x5  (new FRA_(today, Date::AddMonths(today, 2),  Date::AddMonths(today, 5),  0.0, libor3m));
        const Handle_<YCInstrument_> fra3x6  (new FRA_(today, Date::AddMonths(today, 3),  Date::AddMonths(today, 6),  0.0, libor3m));
        const Handle_<YCInstrument_> fra4x7  (new FRA_(today, Date::AddMonths(today, 4),  Date::AddMonths(today, 7),  0.0, libor3m));
        const Handle_<YCInstrument_> fra5x8  (new FRA_(today, Date::AddMonths(today, 5),  Date::AddMonths(today, 8),  0.0, libor3m));
        const Handle_<YCInstrument_> fra6x9  (new FRA_(today, Date::AddMonths(today, 6),  Date::AddMonths(today, 9),  0.0, libor3m));
        const Handle_<YCInstrument_> fra7x10 (new FRA_(today, Date::AddMonths(today, 7),  Date::AddMonths(today, 10), 0.0, libor3m));
        const Handle_<YCInstrument_> fra8x11 (new FRA_(today, Date::AddMonths(today, 8),  Date::AddMonths(today, 11), 0.0, libor3m));
        const Handle_<YCInstrument_> fra9x12 (new FRA_(today, Date::AddMonths(today, 9),  Date::AddMonths(today, 12), 0.0, libor3m));
        const Handle_<YCInstrument_> fra12x15(new FRA_(today, Date::AddMonths(today, 12), Date::AddMonths(today, 15), 0.0, libor3m));
        const Handle_<YCInstrument_> irs18m(new Swap_(today, today, Date::AddMonths(today, 18),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs2y (new Swap_(today, today, Date::AddMonths(today, 24),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs30m(new Swap_(today, today, Date::AddMonths(today, 30),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs3y (new Swap_(today, today, Date::AddMonths(today, 36),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs4y (new Swap_(today, today, Date::AddMonths(today, 48),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs5y (new Swap_(today, today, Date::AddMonths(today, 60),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs6y (new Swap_(today, today, Date::AddMonths(today, 72),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs7y (new Swap_(today, today, Date::AddMonths(today, 84),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs8y (new Swap_(today, today, Date::AddMonths(today, 96),  0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs10y(new Swap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, libor3m, floatLeg));
        const Vector_<String_> liborNames = {
            "FRA 1x4", "FRA 2x5", "FRA 3x6", "FRA 4x7", "FRA 5x8", "FRA 6x9",
            "FRA 7x10", "FRA 8x11", "FRA 9x12", "FRA 12x15",
            "IRS 18M", "IRS 2Y", "IRS 30M", "IRS 3Y", "IRS 4Y", "IRS 5Y",
            "IRS 6Y", "IRS 7Y", "IRS 8Y", "IRS 10Y",
        };


        // -- OIS stage (20 instruments) --
        CurveCalibrationSpec_ oisStage;
        oisStage.today_ = today;
        oisStage.ccy_ = ccyName;
        oisStage.curveName_ = "ois";
        oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisStage.knotDates_ = {
            Date::AddMonths(today, 1),   Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),   Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),  Date::AddMonths(today, 36),
            Date::AddMonths(today, 60),  Date::AddMonths(today, 84),
            Date::AddMonths(today, 120),
        };
        oisStage.instruments_ = {
            QuotedInstrument(oisDep1w,  marketCurve, today, ccy),
            QuotedInstrument(oisDep2w,  marketCurve, today, ccy),
            QuotedInstrument(oisDep1m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep2m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep3m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep4m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep5m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep6m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep9m,  marketCurve, today, ccy),
            QuotedInstrument(oisDep12m, marketCurve, today, ccy),
            QuotedInstrument(oisSwap18m,marketCurve, today, ccy),
            QuotedInstrument(oisSwap2y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap30m,marketCurve, today, ccy),
            QuotedInstrument(oisSwap3y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap4y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap5y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap6y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap7y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap8y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap10y,marketCurve, today, ccy),
        };

        // -- Libor 3M stage (20 instruments) --
        CurveCalibrationSpec_ liborStage;
        liborStage.today_ = today;
        liborStage.ccy_ = ccyName;
        liborStage.curveName_ = "libor3m";
        liborStage.calibrateDiscountCurve_ = false;
        liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborStage.targetTenor_ = libor3m.forecastTenor_;
        liborStage.knotDates_ = oisStage.knotDates_;
        liborStage.instruments_ = {
            QuotedInstrument(fra1x4,  marketCurve, today, ccy),
            QuotedInstrument(fra2x5,  marketCurve, today, ccy),
            QuotedInstrument(fra3x6,  marketCurve, today, ccy),
            QuotedInstrument(fra4x7,  marketCurve, today, ccy),
            QuotedInstrument(fra5x8,  marketCurve, today, ccy),
            QuotedInstrument(fra6x9,  marketCurve, today, ccy),
            QuotedInstrument(fra7x10, marketCurve, today, ccy),
            QuotedInstrument(fra8x11, marketCurve, today, ccy),
            QuotedInstrument(fra9x12, marketCurve, today, ccy),
            QuotedInstrument(fra12x15,marketCurve, today, ccy),
            QuotedInstrument(irs18m,  marketCurve, today, ccy),
            QuotedInstrument(irs2y,   marketCurve, today, ccy),
            QuotedInstrument(irs30m,  marketCurve, today, ccy),
            QuotedInstrument(irs3y,   marketCurve, today, ccy),
            QuotedInstrument(irs4y,   marketCurve, today, ccy),
            QuotedInstrument(irs5y,   marketCurve, today, ccy),
            QuotedInstrument(irs6y,   marketCurve, today, ccy),
            QuotedInstrument(irs7y,   marketCurve, today, ccy),
            QuotedInstrument(irs8y,   marketCurve, today, ccy),
            QuotedInstrument(irs10y,  marketCurve, today, ccy),
        };

        MultiCurveCalibrationSpec_ multiCurveSpec;
        multiCurveSpec.name_ = "usd_example";
        multiCurveSpec.ccy_ = ccyName;
        multiCurveSpec.liborBasis_ = libor3m.dayBasis_;
        multiCurveSpec.stages_ = {oisStage, liborStage};

        const int totalInstruments = static_cast<int>(
            oisStage.instruments_.size() + liborStage.instruments_.size());

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Sequential multi-curve calibration example  ("
                  << totalInstruments << " instruments)\n"
                  << std::string(70, '=') << "\n";

        Timer_ timer;
        timer.Reset();
        const auto result = CalibrateMultiCurve(multiCurveSpec);
        const auto totalMs = timer.Elapsed<milliseconds>();
        const double perStageMs = static_cast<double>(totalMs) / 2.0;
        const CurveBlock_ calibratedCurve("usd_example", ccyName, result.discountCurves_, result.forwardCurves_, libor3m.dayBasis_);

        PrintStageDiagnostics(result.diagnostics_[0], oisNames,   perStageMs);
        PrintStageDiagnostics(result.diagnostics_[1], liborNames, perStageMs);

        std::cout << "  Total calibration elapsed: " << int(totalMs) << " ms\n\n";

        const auto fraRate = fra3x6->Precompute(Handle_<YieldCurve_>());
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  Forward 3x6 FRA repriced on calibrated bundle: " << (*fraRate)(calibratedCurve) * 100.0 << "%\n";
    }
} // namespace

int main() {
    RegisterAll_::Init();

    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");

    PrintScheduleExample(today, ccy);
    PrintScheduleContextExample();
    PrintForwardInstrumentExample(today, ccy);
    PrintMultiCurveExample(today, ccy);

    return 0;
}
