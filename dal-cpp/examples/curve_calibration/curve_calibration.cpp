//
// Created by GitHub Copilot on 2026/5/19.
//

#include <iomanip>
#include <iostream>
#include <memory>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/currency/currencydata.hpp>
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
        const auto rate = prototype->Precompute(prototype, Handle_<YieldCurve_>());

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

        const auto futureRate = future->Precompute(future, Handle_<YieldCurve_>());
        const auto basisSwapRate = basisSwap->Precompute(basisSwap, Handle_<YieldCurve_>());

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

    void PrintStageDiagnostics(const CurveCalibrationDiagnostics_& diagnostics) {
        std::cout << "\n  " << diagnostics.curveName_ << " calibration residuals\n";
        std::cout << "  " << std::string(36, '-') << "\n\n";
        const Vector_<int> w = {22, 12, 12, 12};
        std::cout << std::left  << std::setw(w[0]) << "Instrument"
                  << std::right << std::setw(w[1]) << "Market(%)"
                  << std::setw(w[2]) << "Model(%)"
                  << std::setw(w[3]) << "Error(bp)" << '\n';
        std::cout << std::string(58, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(diagnostics.instrumentNames_.size()); ++i) {
            std::cout << std::left  << std::setw(w[0]) << diagnostics.instrumentNames_[i]
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

        // OIS instruments — deposit-like (short end) + swaps (long end)
        const Handle_<YCInstrument_> oisDep1w(new Deposit_(today, today, today.AddDays(7), 0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep1m(new Deposit_(today, today, Date::AddMonths(today, 1), 0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep3m(new Deposit_(today, today, Date::AddMonths(today, 3), 0.0, overnightIndex));
        const Handle_<YCInstrument_> oisDep6m(new Deposit_(today, today, Date::AddMonths(today, 6), 0.0, overnightIndex));
        const Handle_<YCInstrument_> oisSwap1y(new OISSwap_(today, today, Date::AddMonths(today, 12), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap2y(new OISSwap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap3y(new OISSwap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap5y(new OISSwap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap7y(new OISSwap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> oisSwap10y(new OISSwap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, overnightIndex, overnightLeg));
        // Libor instruments — FRAs (short end) + IRS (long end)
        const Handle_<YCInstrument_> fra1x4(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.0, libor3m));
        const Handle_<YCInstrument_> fra3x6(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, libor3m));
        const Handle_<YCInstrument_> fra6x9(new FRA_(today, Date::AddMonths(today, 6), Date::AddMonths(today, 9), 0.0, libor3m));
        const Handle_<YCInstrument_> fra9x12(new FRA_(today, Date::AddMonths(today, 9), Date::AddMonths(today, 12), 0.0, libor3m));
        const Handle_<YCInstrument_> fra12x15(new FRA_(today, Date::AddMonths(today, 12), Date::AddMonths(today, 15), 0.0, libor3m));
        const Handle_<YCInstrument_> irs2y(new Swap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs3y(new Swap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs5y(new Swap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs7y(new Swap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, libor3m, floatLeg));
        const Handle_<YCInstrument_> irs10y(new Swap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, libor3m, floatLeg));

        CurveCalibrationSpec_ oisStage;
        oisStage.today_ = today;
        oisStage.ccy_ = ccyName;
        oisStage.curveName_ = "ois";
        oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisStage.knotDates_ = {
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
        oisStage.instruments_ = {
            QuotedInstrument(oisDep1w, marketCurve, today, ccy),
            QuotedInstrument(oisDep1m, marketCurve, today, ccy),
            QuotedInstrument(oisDep3m, marketCurve, today, ccy),
            QuotedInstrument(oisDep6m, marketCurve, today, ccy),
            QuotedInstrument(oisSwap1y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap2y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap3y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap5y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap7y, marketCurve, today, ccy),
            QuotedInstrument(oisSwap10y, marketCurve, today, ccy),
        };

        CurveCalibrationSpec_ liborStage;
        liborStage.today_ = today;
        liborStage.ccy_ = ccyName;
        liborStage.curveName_ = "libor3m";
        liborStage.calibrateDiscountCurve_ = false;
        liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborStage.targetTenor_ = libor3m.forecastTenor_;
        liborStage.knotDates_ = oisStage.knotDates_;
        liborStage.instruments_ = {
            QuotedInstrument(fra1x4, marketCurve, today, ccy),
            QuotedInstrument(fra3x6, marketCurve, today, ccy),
            QuotedInstrument(fra6x9, marketCurve, today, ccy),
            QuotedInstrument(fra9x12, marketCurve, today, ccy),
            QuotedInstrument(fra12x15, marketCurve, today, ccy),
            QuotedInstrument(irs2y, marketCurve, today, ccy),
            QuotedInstrument(irs3y, marketCurve, today, ccy),
            QuotedInstrument(irs5y, marketCurve, today, ccy),
            QuotedInstrument(irs7y, marketCurve, today, ccy),
            QuotedInstrument(irs10y, marketCurve, today, ccy),
        };

        MultiCurveCalibrationSpec_ multiCurveSpec;
        multiCurveSpec.name_ = "usd_example";
        multiCurveSpec.ccy_ = ccyName;
        multiCurveSpec.liborBasis_ = libor3m.dayBasis_;
        multiCurveSpec.stages_ = {oisStage, liborStage};

        const auto result = CalibrateMultiCurve(multiCurveSpec);
        const CurveBlock_ calibratedCurve("usd_example", ccyName, result.discountCurves_, result.forwardCurves_, libor3m.dayBasis_);

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Sequential multi-curve calibration example\n"
                  << std::string(70, '=') << "\n";
        for (const auto& diagnostics : result.diagnostics_)
            PrintStageDiagnostics(diagnostics);

        const auto fraRate = fra3x6->Precompute(fra3x6, Handle_<YieldCurve_>());
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "Forward 3x6 FRA repriced on calibrated bundle: " << (*fraRate)(calibratedCurve) * 100.0 << "%\n";
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
