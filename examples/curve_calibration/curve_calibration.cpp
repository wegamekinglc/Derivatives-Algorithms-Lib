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
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
            Date::AddMonths(today, 60),
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

        std::cout << "Convention-based schedule example\n";
        std::cout << "---------------------------------\n";
        std::cout << std::left << std::setw(14) << "AccrualStart"
                  << std::setw(14) << "AccrualEnd"
                  << std::setw(14) << "PaymentDate"
                  << std::setw(10) << "Stub" << '\n';
        for (const auto& period : periods) {
            std::cout << std::setw(14) << Date::ToString(period.accrualStart_)
                      << std::setw(14) << Date::ToString(period.accrualEnd_)
                      << std::setw(14) << Date::ToString(period.paymentDate_)
                      << std::setw(10) << (period.isStub_ ? "yes" : "no") << '\n';
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

        std::cout << "Schedule context example with fixing/payment lags\n";
        std::cout << "-----------------------------------------------\n";
        std::cout << std::left << std::setw(14) << "FixingDate"
                  << std::setw(14) << "AccrualEnd"
                  << std::setw(14) << "PaymentDate"
                  << std::setw(10) << "Months"
                  << std::setw(10) << "Stub" << '\n';
        for (const auto& period : periods) {
            std::cout << std::setw(14) << Date::ToString(period.fixingDate_)
                      << std::setw(14) << Date::ToString(period.accrualEnd_)
                      << std::setw(14) << Date::ToString(period.paymentDate_)
                      << std::setw(10)
                      << (period.dayCountContext_ ? period.dayCountContext_->couponMonths_ : 0)
                      << std::setw(10) << (period.isStub_ ? "yes" : "no") << '\n';
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

        std::cout << "Forward instrument routing example\n";
        std::cout << "---------------------------------\n";
        std::cout << "3M future rate with 15bp convexity adjustment: "
                  << (*futureRate)(curve) * 100.0 << "%\n";
        std::cout << "3M vs 6M basis swap spread from separate forward curves: "
                  << (*basisSwapRate)(curve) * 10000.0 << " bp\n\n";
    }

    void PrintStageDiagnostics(const CurveCalibrationDiagnostics_& diagnostics) {
        std::cout << diagnostics.curveName_ << " residuals\n";
        std::cout << std::left << std::setw(14) << "Instrument"
                  << std::setw(12) << "Market"
                  << std::setw(12) << "Model"
                  << std::setw(12) << "Error(bp)" << '\n';
        for (int i = 0; i < static_cast<int>(diagnostics.instrumentNames_.size()); ++i) {
            std::cout << std::setw(14) << diagnostics.instrumentNames_[i]
                      << std::setw(12) << diagnostics.marketRates_[i] * 100.0
                      << std::setw(12) << diagnostics.modelRates_[i] * 100.0
                      << std::setw(12) << diagnostics.residuals_[i] * 10000.0 << '\n';
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

        const Handle_<YCInstrument_> oisDeposit(new Deposit_(today, today, Date::AddMonths(today, 3), 0.0, overnightIndex));
        const Handle_<YCInstrument_> oisSwap(new OISSwap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, overnightIndex, overnightLeg));
        const Handle_<YCInstrument_> fra3m(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, libor3m));
        const Handle_<YCInstrument_> irs3m(
            new Swap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, libor3m, floatLeg));

        CurveCalibrationSpec_ oisStage;
        oisStage.today_ = today;
        oisStage.ccy_ = ccyName;
        oisStage.curveName_ = "ois";
        oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisStage.knotDates_ = {
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
        };
        oisStage.instruments_ = {
            QuotedInstrument(oisDeposit, marketCurve, today, ccy),
            QuotedInstrument(oisSwap, marketCurve, today, ccy),
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
            QuotedInstrument(fra3m, marketCurve, today, ccy),
            QuotedInstrument(irs3m, marketCurve, today, ccy),
        };

        MultiCurveCalibrationSpec_ multiCurveSpec;
        multiCurveSpec.name_ = "usd_example";
        multiCurveSpec.ccy_ = ccyName;
        multiCurveSpec.liborBasis_ = libor3m.dayBasis_;
        multiCurveSpec.stages_ = {oisStage, liborStage};

        const auto result = CalibrateMultiCurve(multiCurveSpec);
        const CurveBlock_ calibratedCurve("usd_example", ccyName, result.discountCurves_, result.forwardCurves_, libor3m.dayBasis_);

        std::cout << "Sequential multi-curve calibration example\n";
        std::cout << "-----------------------------------------\n";
        for (const auto& diagnostics : result.diagnostics_)
            PrintStageDiagnostics(diagnostics);

        const auto fraRate = fra3m->Precompute(fra3m, Handle_<YieldCurve_>());
        std::cout << "Forward 3M FRA repriced on calibrated bundle: " << (*fraRate)(calibratedCurve) * 100.0 << "%\n";
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
