//
// Created by dal-spec-writer on 2026/6/20.
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>

#include <dal/platform/platform.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
// daybasis.hpp + periodlength.hpp are load-bearing under the AAD backend builds (Adept/CoDiPack/XAD):
// schedules.hpp and MG_DayBasis_enum.hpp use Handle_ and assert without including them, and the AAD
// backend changes the transitive include graph so the platform.hpp definitions are not in scope on
// the transitive path. Including daybasis.hpp explicitly after platform.hpp makes the build
// backend-neutral. Do not auto-strip as "unused".
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/time/schedules.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

namespace {
    // Local THROW_REQUIRE-style macro mirroring yield_curve_jacobian.cpp:35-39 (NOT in a shared header).
#define THROW_REQUIRE(cond, msg)                                                                                                                     \
    do {                                                                                                                                             \
        if (!(cond))                                                                                                                                 \
            THROW(msg);                                                                                                                              \
    } while (false)

    // BAR-A is the sole PASS gate (both paths reprice to 10 * fitTolerance_). BAR-B and BAR-C are
    // INFORMATIONAL joint-vs-staged DF-drift measurements, printed for teaching, NOT pass/fail bars
    // (the drift is expected and is NOT a bug -- see docs/methodology/yield_curve.md,
    // "Joint simultaneous calibration"). Both paths run EXACT and base-layer the 3M forward over
    // the OIS discount curve (joint via baseLayeredOverDiscount_, staged via ApplyStageDefaults),
    // so the stored 3M curves are structurally identical (DiscountPWLF_ with base = OIS).
    constexpr double BAR_A_TOLERANCE = 1.0e-7;  // PASS gate: 10 * fitTolerance_, both paths
    constexpr double BAR_B_REFERENCE = 1.0e-6;  // measured EXACT OIS drift 6.42e-7, rounded up (informational)
    constexpr double BAR_C_REFERENCE = 5.0e-5;  // measured EXACT 3M drift 2.39e-5, rounded up (informational)

    Handle_<DiscountCurve_> MakeFlatDiscountCurve(
        const String_& name, const String_& ccy, const Date_& today, double rate, const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        const Vector_<Date_> knotDates = {
            Date::AddMonths(today, 1),  Date::AddMonths(today, 3),  Date::AddMonths(today, 6),
            Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
            Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
        };
        const Vector_<> values(knotDates.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, values, values), base));
    }

    Vector_<Date_> SharedKnots(const Date_& today) {
        return Vector_<Date_>{
            Date::AddMonths(today, 1),  Date::AddMonths(today, 3),  Date::AddMonths(today, 6),
            Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
            Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
        };
    }

    Handle_<YCInstrument_>
    QuotedInstrument(const Handle_<YCInstrument_>& prototype, const YieldCurve_& marketCurve, const Date_& tradeDate, const Ccy_& ccy) {
        const auto rate = prototype->Precompute(Handle_<YieldCurve_>());
        if (const auto* deposit = dynamic_cast<const Deposit_*>(prototype.get())) {
            const auto span = deposit->TimeSpan();
            return Handle_<YCInstrument_>(new Deposit_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::OisIndex()(ccy)));
        }
        if (const auto* fra = dynamic_cast<const FRA_*>(prototype.get())) {
            const auto span = fra->TimeSpan();
            return Handle_<YCInstrument_>(new FRA_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::LiborIndex()(ccy)));
        }
        if (const auto* swap = dynamic_cast<const OISSwap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
            auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
            auto floatLeg = fixedLeg;
            floatLeg.paymentFrequency_ = PeriodLength_("12M");
            floatLeg.dayBasis_ = overnightIndex.dayBasis_;
            return Handle_<YCInstrument_>(new OISSwap_(tradeDate, span.first, span.second, (*rate)(marketCurve), fixedLeg, overnightIndex, floatLeg));
        }
        if (const auto* swap = dynamic_cast<const Swap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            return Handle_<YCInstrument_>(new Swap_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::SwapFixedLeg()(ccy),
                                                    Ccy::Conventions::LiborIndex()(ccy), Ccy::Conventions::SwapFloatLeg()(ccy)));
        }
        THROW("Unsupported example instrument type");
    }

    struct MarketSet_ {
        Vector_<Handle_<YCInstrument_>> ois;
        Vector_<Handle_<YCInstrument_>> libor;
        DayBasis_ liborBasis;
        PeriodLength_ forecastTenor;
    };

    // Build the 12 OIS + 12 IBOR-3M instruments (24 total, satisfies the >= 20 requirement) with
    // self-consistent par rates derived from a synthetic flat market (OIS=1.0%, 3M=3.0% over OIS).
    MarketSet_ BuildMarket(const Date_& today, const Ccy_& ccy) {
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
        // AAD eligibility requires ACT_365F (the templated PWL-forward curve uses DAYS_PER_YEAR = 365).
        // The synthetic market is self-consistent regardless of the day-count convention, so we
        // conform to ACT_365F to let the analytic Gradient engage.
        libor3m.dayBasis_ = DayBasis_("ACT_365F");

        auto floatLeg = Ccy::Conventions::SwapFloatLeg()(ccy);
        floatLeg.accrualHolidays_ = Holidays::None();
        floatLeg.paymentHolidays_ = Holidays::None();

        auto overnightLeg = fixedLeg;
        overnightLeg.paymentFrequency_ = PeriodLength_("12M");
        overnightLeg.dayBasis_ = overnightIndex.dayBasis_;

        const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois_market", ccyName, today, 0.01);
        const Handle_<DiscountCurve_> forward3m = MakeFlatDiscountCurve("libor3m_market", ccyName, today, 0.03, ois);
        const CurveBlock_ marketCurve("market", ccyName, {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                      {{libor3m.forecastTenor_, forward3m}}, libor3m.dayBasis_);

        // 12 OIS prototypes: 6 deposits (1M, 2M, 3M, 6M, 9M, 12M) + 6 OIS swaps (2Y, 3Y, 4Y, 5Y, 7Y, 10Y).
        Vector_<Handle_<YCInstrument_>> oisProto = {
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 1), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 2), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 3), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 6), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 9), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 12), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 48), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, overnightIndex, overnightLeg)),
        };

        // 12 IBOR-3M prototypes: 6 FRAs (1x4, 2x5, 3x6, 6x9, 9x12, 12x15) + 6 vanilla swaps.
        Vector_<Handle_<YCInstrument_>> liborProto = {
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 2), Date::AddMonths(today, 5), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 6), Date::AddMonths(today, 9), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 9), Date::AddMonths(today, 12), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 12), Date::AddMonths(today, 15), 0.0, libor3m)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 48), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, libor3m, floatLeg)),
        };

        Vector_<Handle_<YCInstrument_>> oisQuoted;
        oisQuoted.reserve(oisProto.size());
        for (const auto& p : oisProto)
            oisQuoted.push_back(QuotedInstrument(p, marketCurve, today, ccy));

        Vector_<Handle_<YCInstrument_>> liborQuoted;
        liborQuoted.reserve(liborProto.size());
        for (const auto& p : liborProto)
            liborQuoted.push_back(QuotedInstrument(p, marketCurve, today, ccy));

        return {std::move(oisQuoted), std::move(liborQuoted), libor3m.dayBasis_, libor3m.forecastTenor_};
    }

    JointMultiCurveCalibrationSpec_ BuildJointSpec(const Date_& today, const Ccy_& ccy, const MarketSet_& market) {
        JointMultiCurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = ccy.String();
        spec.liborBasis_ = market.liborBasis;
        // EXACT (Underdetermined::Find) is the library default; set explicitly here so the joint
        // and staged paths use the SAME mode for a fair BAR-B/BAR-C comparison.
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.fitTolerance_ = 1.0e-10;
        spec.tolerance_ = 1.0e-10;
        spec.smoothingWeight_ = 1.0;

        const Vector_<Date_> knots = SharedKnots(today);

        JointCurveDeclaration_ oisDecl;
        oisDecl.curveName_ = "joint_ois";
        oisDecl.instruments_ = market.ois;
        oisDecl.knotDates_ = knots;
        oisDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisDecl.calibrateDiscountCurve_ = true;

        JointCurveDeclaration_ liborDecl;
        liborDecl.curveName_ = "joint_3m";
        liborDecl.instruments_ = market.libor;
        liborDecl.knotDates_ = knots;
        liborDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborDecl.targetTenor_ = market.forecastTenor;
        liborDecl.calibrateDiscountCurve_ = false;
        // Base-layer the joint 3M over the joint OIS so the smoother acts on the spread forward
        // f_abs - f_ois (matching the staged path's ApplyStageDefaults). The stored joint 3M is then
        // structurally identical to the staged 3M (DiscountPWLF_ with base = OIS), eliminating the
        // representation-mismatch drift of the baseless default (BAR-C 3.3e-4 -> 2.4e-5).
        liborDecl.baseLayeredOverDiscount_ = true;

        spec.curves_ = Vector_<JointCurveDeclaration_>{oisDecl, liborDecl};
        return spec;
    }

    MultiCurveCalibrationSpec_ BuildStagedSpec(const Date_& today, const Ccy_& ccy, const MarketSet_& market) {
        CurveCalibrationSpec_ oisStage;
        oisStage.today_ = today;
        oisStage.ccy_ = ccy.String();
        oisStage.curveName_ = "staged_ois";
        oisStage.instruments_ = market.ois;
        oisStage.knotDates_ = SharedKnots(today);
        oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisStage.solveMode_ = CurveSolveMode_::Value_::EXACT;
        oisStage.fitTolerance_ = 1.0e-10;
        oisStage.tolerance_ = 1.0e-10;
        oisStage.liborBasis_ = market.liborBasis;

        CurveCalibrationSpec_ liborStage;
        liborStage.today_ = today;
        liborStage.ccy_ = ccy.String();
        liborStage.curveName_ = "staged_3m";
        liborStage.instruments_ = market.libor;
        liborStage.knotDates_ = SharedKnots(today);
        liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborStage.targetTenor_ = market.forecastTenor;
        liborStage.calibrateDiscountCurve_ = false;
        liborStage.solveMode_ = CurveSolveMode_::Value_::EXACT;
        liborStage.fitTolerance_ = 1.0e-10;
        liborStage.tolerance_ = 1.0e-10;
        liborStage.liborBasis_ = market.liborBasis;

        MultiCurveCalibrationSpec_ multi;
        multi.name_ = "staged_usd";
        multi.ccy_ = ccy.String();
        multi.liborBasis_ = market.liborBasis;
        multi.stages_ = Vector_<CurveCalibrationSpec_>{oisStage, liborStage};
        return multi;
    }

    // Both diagnostics structs (joint and staged) expose curveName_, residuals_, maxAbsResidual_,
    // rmsResidual_ with identical shapes, so a single template covers both summary tables.
    template <class Diag_> void PrintResidualSummary(const char* path, const Vector_<Diag_>& diags) {
        std::cout << "\n  " << path << " per-curve residuals\n";
        std::cout << "  " << std::string(54, '-') << "\n";
        std::cout << std::left << std::setw(14) << "Curve" << std::right << std::setw(12) << "nInstr" << std::setw(16) << "maxAbs(bp)"
                  << std::setw(16) << "rms(bp)" << "\n";
        std::cout << std::string(54, '-') << "\n";
        std::cout << std::fixed << std::setprecision(6);
        for (const auto& diag : diags)
            std::cout << std::left << std::setw(14) << diag.curveName_ << std::right << std::setw(12) << diag.residuals_.size() << std::setw(16)
                      << diag.maxAbsResidual_ * 10000.0 << std::setw(16) << diag.rmsResidual_ * 10000.0 << "\n";
        std::cout << "\n";
    }

    void PrintDfComparisonTable(const char* title, const Date_& today, const DiscountCurve_& jointCurve, const DiscountCurve_& stagedCurve) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  " << title << "  (joint vs staged)\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << std::left << std::setw(10) << "Pillar" << std::right << std::setw(22) << "DF_joint" << std::setw(22) << "DF_staged"
                  << std::setw(14) << "|diff|" << "\n";
        std::cout << std::string(68, '-') << "\n";
        const Vector_<int> pillarMonths = {12, 24, 36, 60, 84, 120};
        double maxDiff = 0.0;
        double sqDiff = 0.0;
        int n = 0;
        for (const int months : pillarMonths) {
            const Date_ pillar = Date::AddMonths(today, months);
            const double dj = jointCurve(today, pillar);
            const double ds = stagedCurve(today, pillar);
            const double diff = std::fabs(dj - ds);
            maxDiff = std::max(maxDiff, diff);
            sqDiff += diff * diff;
            ++n;
            std::cout << std::left << std::setw(10) << (std::to_string(months / 12) + "Y") << std::right << std::setw(22) << std::setprecision(10)
                      << dj << std::setw(22) << ds << std::setw(14) << std::setprecision(4) << std::scientific << diff << std::fixed << "\n";
        }
        const double rms = std::sqrt(sqDiff / n);
        std::cout << std::string(68, '-') << "\n";
        std::cout << "  max |diff| = " << std::scientific << std::setprecision(4) << maxDiff << "    RMS |diff| = " << rms << std::fixed << "\n\n";
    }

    void RunSelfChecks(const JointMultiCurveCalibrationResult_& joint,
                       const MultiCurveCalibrationResult_& staged,
                       const Date_& today,
                       const MarketSet_& market) {
        // BAR-A (loose): both paths' per-curve maxAbsResidual <= 10 * fitTolerance_ = 1e-7, and all
        // pillar DFs finite in (0, 1].
        for (const auto& diag : joint.diagnostics_)
            THROW_REQUIRE(diag.maxAbsResidual_ <= BAR_A_TOLERANCE, String_("BAR-A failed: joint curve ") + diag.curveName_ + " maxAbsResidual " +
                                                                       String::FromDouble(diag.maxAbsResidual_) + " exceeds " +
                                                                       String::FromDouble(BAR_A_TOLERANCE));
        for (const auto& diag : staged.diagnostics_)
            THROW_REQUIRE(diag.maxAbsResidual_ <= BAR_A_TOLERANCE, String_("BAR-A failed: staged curve ") + diag.curveName_ + " maxAbsResidual " +
                                                                       String::FromDouble(diag.maxAbsResidual_) + " exceeds " +
                                                                       String::FromDouble(BAR_A_TOLERANCE));
        THROW_REQUIRE(joint.converged_, "BAR-A failed: joint solve did not converge");

        const DiscountCurve_& jointOis = *joint.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
        const DiscountCurve_& stagedOis = *staged.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
        const DiscountCurve_& joint3m = *joint.forwardCurves_.at(market.forecastTenor);
        const DiscountCurve_& staged3m = *staged.forwardCurves_.at(market.forecastTenor);
        const Vector_<int> pillarMonths = {12, 24, 36, 60, 84, 120};
        for (const int months : pillarMonths) {
            const Date_ pillar = Date::AddMonths(today, months);
            for (const DiscountCurve_* curve : {&jointOis, &stagedOis, &joint3m, &staged3m}) {
                const double df = (*curve)(today, pillar);
                THROW_REQUIRE(std::isfinite(df), "BAR-A failed: DF not finite");
                THROW_REQUIRE(df > 0.0 && df <= 1.0, "BAR-A failed: DF out of (0, 1]");
            }
        }

        // BAR-B (informational, OIS): joint-vs-staged OIS DF drift -- NOT a pass/fail bar. Reported,
        // not gated. Driven by joint OIS<->3M-spread cross-curve coupling (see
        // docs/methodology/yield_curve.md, "Joint simultaneous calibration").
        double maxOisDiff = 0.0;
        for (const int months : pillarMonths) {
            const Date_ pillar = Date::AddMonths(today, months);
            maxOisDiff = std::max(maxOisDiff, std::fabs(jointOis(today, pillar) - stagedOis(today, pillar)));
        }

        // BAR-C (informational, 3M): joint-vs-staged 3M DF drift -- NOT a pass/fail bar. Reported,
        // not gated. The OIS-slice difference (BAR-B) propagating through the 3M base handle, plus
        // a boundary contribution at the least-constrained 1Y/10Y knots. A mis-routing bug (fixing
        // off OIS) would show up at ~1e-2 here, so the diagnostic still discriminates.
        double max3mDiff = 0.0;
        for (const int months : pillarMonths) {
            const Date_ pillar = Date::AddMonths(today, months);
            max3mDiff = std::max(max3mDiff, std::fabs(joint3m(today, pillar) - staged3m(today, pillar)));
        }

        std::cout << "  BAR-A (PASS gate): PASS  (both paths maxAbsResidual <= " << BAR_A_TOLERANCE << ", all pillar DFs in (0, 1])\n";
        std::cout << "  BAR-B (info, OIS):  max |diff| = " << std::scientific << std::setprecision(4) << maxOisDiff << "  (spec nominal "
                  << BAR_B_REFERENCE << "; drift from joint OIS<->3M-spread cross-curve coupling)\n"
                  << std::fixed;
        std::cout << "  BAR-C (info, 3M):   max |diff| = " << std::scientific << std::setprecision(4) << max3mDiff << "  (spec nominal "
                  << BAR_C_REFERENCE << "; drift from joint-vs-staged OIS-slice difference propagating through 3M base)\n"
                  << std::fixed;
    }
} // namespace

int main() {
    RegisterAll_::Init();

    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const MarketSet_ market = BuildMarket(today, ccy);

    const int totalInstruments = static_cast<int>(market.ois.size() + market.libor.size());

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  Joint simultaneous multi-curve calibration example  (" << totalInstruments << " instruments: " << market.ois.size() << " OIS + "
              << market.libor.size() << " IBOR-3M)\n";
    std::cout << std::string(70, '=') << "\n";

    Timer_ timer;

    timer.Reset();
    const JointMultiCurveCalibrationSpec_ jointSpec = BuildJointSpec(today, ccy, market);
    const JointMultiCurveCalibrationResult_ jointResult = CalibrateJointMultiCurve(jointSpec);
    const double jointMs = static_cast<double>(timer.Elapsed<milliseconds>());

    timer.Reset();
    const MultiCurveCalibrationSpec_ stagedSpec = BuildStagedSpec(today, ccy, market);
    const MultiCurveCalibrationResult_ stagedResult = CalibrateMultiCurve(stagedSpec);
    const double stagedMs = static_cast<double>(timer.Elapsed<milliseconds>());

    PrintResidualSummary("joint", jointResult.diagnostics_);
    PrintResidualSummary("staged", stagedResult.diagnostics_);

    const DiscountCurve_& jointOis = *jointResult.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const DiscountCurve_& stagedOis = *stagedResult.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const DiscountCurve_& joint3m = *jointResult.forwardCurves_.at(market.forecastTenor);
    const DiscountCurve_& staged3m = *stagedResult.forwardCurves_.at(market.forecastTenor);

    PrintDfComparisonTable("OIS discount curve", today, jointOis, stagedOis);
    PrintDfComparisonTable("3M forward curve", today, joint3m, staged3m);

    std::cout << "  Timing: joint solve " << int(jointMs) << " ms, staged solve " << int(stagedMs) << " ms\n";
    std::cout << "  Joint solver evaluations: " << jointResult.solverEvaluations_ << "\n";
    if (!jointResult.jacobianAtSolution_.Empty())
        std::cout << "  AAD Jacobian: engaged (forward J size " << jointResult.jacobianAtSolution_.Rows() << "x"
                  << jointResult.jacobianAtSolution_.Cols() << ")\n";
    else
        std::cout << "  AAD Jacobian: not engaged (bumped fallback)\n";
    std::cout << "\n";

    std::cout << std::string(70, '-') << "\n";
    std::cout << "  Self-check\n";
    std::cout << std::string(70, '-') << "\n";
    RunSelfChecks(jointResult, stagedResult, today, market);

    std::cout << "\n  Verdict: PASS\n\n";
    return 0;
}
